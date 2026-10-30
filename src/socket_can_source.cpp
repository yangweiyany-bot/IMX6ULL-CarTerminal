#include "car_terminal/socket_can_source.h"

#include <stdexcept>
#include <utility>

namespace car_terminal {

SocketCanDataSource::SocketCanDataSource(
    ICanFrameReceiver& receiver,
    CanVehicleDataParser& parser,
    std::chrono::milliseconds receiveTimeout,
    StopFunction stopRequested)
    : receiver_(receiver),
      parser_(parser),
      receiveTimeout_(receiveTimeout),
      stopRequested_(std::move(stopRequested)) {
    if (receiveTimeout_.count() <= 0) {
        throw std::invalid_argument(
            "SocketCAN receive timeout must be positive");
    }
    if (!stopRequested_) {
        throw std::invalid_argument(
            "SocketCAN stop callback must be provided");
    }
}

VehicleData SocketCanDataSource::nextSample() {
    const auto shouldStop = [this] {
        return cancellationRequested_.load() || stopRequested_();
    };
    while (true) {
        const CanReceiveResult result =
            receiver_.receive(receiveTimeout_, shouldStop);
        if (result.status == CanReceiveStatus::Stopped) {
            throw AcquisitionStopped();
        }
        if (result.status == CanReceiveStatus::Timeout) {
            continue;
        }
        if (const auto data = parser_.consume(result.frame)) {
            return *data;
        }
    }
}

void SocketCanDataSource::requestStop() noexcept {
    cancellationRequested_.store(true);
}

}  // namespace car_terminal
