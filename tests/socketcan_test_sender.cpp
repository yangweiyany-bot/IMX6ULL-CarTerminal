#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

class FileDescriptor {
public:
    explicit FileDescriptor(int value) : value_(value) {}
    ~FileDescriptor() {
        if (value_ >= 0) {
            ::close(value_);
        }
    }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    int get() const noexcept { return value_; }

private:
    int value_;
};

std::uint32_t parseIdentifier(const std::string& text) {
    std::size_t parsed = 0;
    const unsigned long value = std::stoul(text, &parsed, 0);
    if (parsed != text.size() || value > CAN_EFF_MASK) {
        throw std::runtime_error("invalid CAN identifier");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint8_t hexDigit(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }
    throw std::runtime_error("invalid hexadecimal payload");
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: socketcan_test_sender <interface> <id> <hex-data>\n";
        return 2;
    }

    try {
        const std::string interfaceName = argv[1];
        const std::uint32_t identifier = parseIdentifier(argv[2]);
        const std::string payload = argv[3];
        if (payload.size() % 2 != 0 || payload.size() > 16) {
            throw std::runtime_error(
                "hex payload must contain 0 to 16 characters in byte pairs");
        }

        const FileDescriptor socketDescriptor(
            ::socket(PF_CAN, SOCK_RAW, CAN_RAW));
        if (socketDescriptor.get() < 0) {
            throw std::runtime_error(std::string("socket: ") +
                                     std::strerror(errno));
        }
        const unsigned int index = ::if_nametoindex(interfaceName.c_str());
        if (index == 0U) {
            throw std::runtime_error(std::string("if_nametoindex: ") +
                                     std::strerror(errno));
        }
        sockaddr_can address{};
        address.can_family = AF_CAN;
        address.can_ifindex = static_cast<int>(index);
        if (::bind(socketDescriptor.get(),
                   reinterpret_cast<const sockaddr*>(&address),
                   sizeof(address)) < 0) {
            throw std::runtime_error(std::string("bind: ") +
                                     std::strerror(errno));
        }

        can_frame frame{};
        frame.can_id = identifier <= CAN_SFF_MASK
                           ? identifier
                           : identifier | CAN_EFF_FLAG;
        frame.len = static_cast<__u8>(payload.size() / 2);
        for (std::size_t indexValue = 0; indexValue < frame.len; ++indexValue) {
            frame.data[indexValue] = static_cast<std::uint8_t>(
                (hexDigit(payload[indexValue * 2]) << 4U) |
                hexDigit(payload[indexValue * 2 + 1]));
        }
        if (::write(socketDescriptor.get(), &frame, sizeof(frame)) !=
            static_cast<ssize_t>(sizeof(frame))) {
            throw std::runtime_error(std::string("write: ") +
                                     std::strerror(errno));
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "socketcan_test_sender: " << error.what() << '\n';
        return 1;
    }
}
