#pragma once

#include <csignal>

namespace car_terminal {

class SignalHandler {
public:
    SignalHandler();
    ~SignalHandler();

    SignalHandler(const SignalHandler&) = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;

    bool stopRequested() const noexcept;
    int signalNumber() const noexcept;

private:
    using HandlerFunction = void (*)(int);

    HandlerFunction previousInterrupt_{};
    HandlerFunction previousTerminate_{};
    bool interruptInstalled_{};
    bool terminateInstalled_{};
};

}  // namespace car_terminal
