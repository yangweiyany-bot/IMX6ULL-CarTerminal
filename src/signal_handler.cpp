#include "car_terminal/signal_handler.h"

#include <stdexcept>

namespace car_terminal {
namespace {

volatile std::sig_atomic_t requestedSignal = 0;

void recordSignal(int signalNumber) noexcept {
    if (requestedSignal == 0) {
        requestedSignal = signalNumber;
    }
}

}  // namespace

SignalHandler::SignalHandler() {
    requestedSignal = 0;

    previousInterrupt_ = std::signal(SIGINT, recordSignal);
    if (previousInterrupt_ == SIG_ERR) {
        throw std::runtime_error("Cannot install SIGINT handler");
    }
    interruptInstalled_ = true;

    previousTerminate_ = std::signal(SIGTERM, recordSignal);
    if (previousTerminate_ == SIG_ERR) {
        std::signal(SIGINT, previousInterrupt_);
        interruptInstalled_ = false;
        throw std::runtime_error("Cannot install SIGTERM handler");
    }
    terminateInstalled_ = true;
}

SignalHandler::~SignalHandler() {
    if (terminateInstalled_) {
        std::signal(SIGTERM, previousTerminate_);
    }
    if (interruptInstalled_) {
        std::signal(SIGINT, previousInterrupt_);
    }
    requestedSignal = 0;
}

bool SignalHandler::stopRequested() const noexcept {
    return requestedSignal != 0;
}

int SignalHandler::signalNumber() const noexcept {
    return static_cast<int>(requestedSignal);
}

}  // namespace car_terminal
