#pragma once

#include "car_terminal/can_parser.h"
#include "car_terminal/monitoring_service.h"
#include "car_terminal/socket_can_receiver.h"

#include <chrono>
#include <atomic>
#include <functional>

namespace car_terminal {

class SocketCanDataSource {
public:
    using StopFunction = std::function<bool()>;

    SocketCanDataSource(ICanFrameReceiver& receiver,
                        CanVehicleDataParser& parser,
                        std::chrono::milliseconds receiveTimeout,
                        StopFunction stopRequested);

    VehicleData nextSample();
    void requestStop() noexcept;

private:
    ICanFrameReceiver& receiver_;
    CanVehicleDataParser& parser_;
    std::chrono::milliseconds receiveTimeout_;
    StopFunction stopRequested_;
    std::atomic<bool> cancellationRequested_{false};
};

}  // namespace car_terminal
