#pragma once

#include "car_terminal/can_types.h"

#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace car_terminal {

class SocketCanError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class CanReceiveStatus {
    Frame,
    Timeout,
    Stopped,
};

struct CanReceiveResult {
    CanReceiveStatus status{CanReceiveStatus::Timeout};
    RawCanFrame frame;
};

class ICanFrameReceiver {
public:
    using StopFunction = std::function<bool()>;

    virtual ~ICanFrameReceiver() = default;
    virtual CanReceiveResult receive(
        std::chrono::milliseconds timeout,
        const StopFunction& stopRequested) = 0;
};

class SocketCanReceiver final : public ICanFrameReceiver {
public:
    SocketCanReceiver(std::string interfaceName,
                      const std::vector<CanFilterRule>& filters);
    ~SocketCanReceiver() override;

    SocketCanReceiver(const SocketCanReceiver&) = delete;
    SocketCanReceiver& operator=(const SocketCanReceiver&) = delete;

    CanReceiveResult receive(
        std::chrono::milliseconds timeout,
        const StopFunction& stopRequested) override;

    const std::string& interfaceName() const noexcept;

private:
    std::string interfaceName_;
    int fileDescriptor_{-1};
};

}  // namespace car_terminal
