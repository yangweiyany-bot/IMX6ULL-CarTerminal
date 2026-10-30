#include "car_terminal/socket_can_receiver.h"

#include "car_terminal/can_parser.h"

#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace car_terminal {
namespace {

std::string systemError(const std::string& operation) {
    return operation + ": " + std::strerror(errno);
}

std::vector<can_filter> makeLinuxFilters(
    const std::vector<CanFilterRule>& filters) {
    if (filters.empty()) {
        throw SocketCanError("SocketCAN requires at least one CAN filter");
    }

    std::vector<can_filter> result;
    result.reserve(filters.size());
    for (const auto& filter : filters) {
        validateCanFilter(filter);
        can_filter linuxFilter{};
        linuxFilter.can_id = filter.identifier;
        linuxFilter.can_mask = filter.mask | CAN_EFF_FLAG;
        if (filter.format == CanFrameFormat::Extended) {
            linuxFilter.can_id |= CAN_EFF_FLAG;
        }
        result.push_back(linuxFilter);
    }
    return result;
}

RawCanFrame convertFrame(const can_frame& frame) {
    if (frame.len > CAN_MAX_DLEN) {
        throw SocketCanError("Classic CAN frame length exceeds 8 bytes");
    }

    RawCanFrame result;
    result.errorFrame = (frame.can_id & CAN_ERR_FLAG) != 0U;
    result.remoteTransmissionRequest = (frame.can_id & CAN_RTR_FLAG) != 0U;
    result.format = (frame.can_id & CAN_EFF_FLAG) != 0U
                        ? CanFrameFormat::Extended
                        : CanFrameFormat::Standard;
    if (result.errorFrame) {
        result.identifier = frame.can_id & CAN_ERR_MASK;
    } else if (result.format == CanFrameFormat::Extended) {
        result.identifier = frame.can_id & CAN_EFF_MASK;
    } else {
        result.identifier = frame.can_id & CAN_SFF_MASK;
    }
    result.dataLength = frame.len;
    std::copy_n(frame.data, frame.len, result.data.begin());
    return result;
}

}  // namespace

SocketCanReceiver::SocketCanReceiver(
    std::string interfaceName,
    const std::vector<CanFilterRule>& filters)
    : interfaceName_(std::move(interfaceName)) {
    if (interfaceName_.empty()) {
        throw SocketCanError("SocketCAN interface name must not be empty");
    }
    if (interfaceName_.size() >= IFNAMSIZ) {
        throw SocketCanError("SocketCAN interface name is too long: " +
                             interfaceName_);
    }

    const auto linuxFilters = makeLinuxFilters(filters);
    fileDescriptor_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fileDescriptor_ < 0) {
        throw SocketCanError(systemError("Cannot create SocketCAN socket"));
    }

    try {
        if (::setsockopt(fileDescriptor_, SOL_CAN_RAW, CAN_RAW_FILTER,
                         linuxFilters.data(),
                         static_cast<socklen_t>(linuxFilters.size() *
                                                sizeof(can_filter))) < 0) {
            throw SocketCanError(
                systemError("Cannot configure SocketCAN filters"));
        }

        const can_err_mask_t errorMask = CAN_ERR_MASK;
        if (::setsockopt(fileDescriptor_, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
                         &errorMask, sizeof(errorMask)) < 0) {
            throw SocketCanError(
                systemError("Cannot configure SocketCAN error filter"));
        }

        errno = 0;
        const unsigned int interfaceIndex =
            ::if_nametoindex(interfaceName_.c_str());
        if (interfaceIndex == 0U) {
            const std::string detail =
                errno == 0 ? "not found" : std::strerror(errno);
            throw SocketCanError("SocketCAN interface is unavailable: " +
                                 interfaceName_ + ": " + detail);
        }

        sockaddr_can address{};
        address.can_family = AF_CAN;
        address.can_ifindex = static_cast<int>(interfaceIndex);
        if (::bind(fileDescriptor_,
                   reinterpret_cast<const sockaddr*>(&address),
                   sizeof(address)) < 0) {
            throw SocketCanError(
                systemError("Cannot bind SocketCAN interface " +
                            interfaceName_));
        }
    } catch (...) {
        ::close(fileDescriptor_);
        fileDescriptor_ = -1;
        throw;
    }
}

SocketCanReceiver::~SocketCanReceiver() {
    if (fileDescriptor_ >= 0) {
        ::close(fileDescriptor_);
    }
}

CanReceiveResult SocketCanReceiver::receive(
    std::chrono::milliseconds timeout,
    const StopFunction& stopRequested) {
    if (timeout.count() <= 0) {
        throw SocketCanError("SocketCAN receive timeout must be positive");
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true) {
        if (stopRequested && stopRequested()) {
            return {CanReceiveStatus::Stopped, {}};
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return {CanReceiveStatus::Timeout, {}};
        }
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now);
        if (remaining.count() <= 0) {
            remaining = std::chrono::milliseconds(1);
        }

        pollfd descriptor{};
        descriptor.fd = fileDescriptor_;
        descriptor.events = POLLIN;
        const int pollResult =
            ::poll(&descriptor, 1, static_cast<int>(remaining.count()));
        if (pollResult == 0) {
            return {CanReceiveStatus::Timeout, {}};
        }
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw SocketCanError(systemError("SocketCAN poll failed"));
        }

        if ((descriptor.revents & POLLIN) != 0) {
            union FrameStorage {
                can_frame classic;
                canfd_frame flexible;
            } storage{};
            const ssize_t received =
                ::recv(fileDescriptor_, &storage, sizeof(storage), 0);
            if (received < 0) {
                if (errno == EINTR || errno == EAGAIN ||
                    errno == EWOULDBLOCK) {
                    continue;
                }
                throw SocketCanError(systemError("SocketCAN receive failed"));
            }
            if (received == CANFD_MTU) {
                throw SocketCanError(
                    "CAN FD frames are not supported; classic CAN is required");
            }
            if (received != CAN_MTU) {
                throw SocketCanError(
                    "SocketCAN returned an unexpected frame size: " +
                    std::to_string(received));
            }
            return {CanReceiveStatus::Frame,
                    convertFrame(storage.classic)};
        }

        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            throw SocketCanError("SocketCAN poll reported an interface error");
        }
    }
}

const std::string& SocketCanReceiver::interfaceName() const noexcept {
    return interfaceName_;
}

}  // namespace car_terminal
