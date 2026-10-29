#pragma once

#include "car_terminal/vehicle_data.h"

#include <chrono>
#include <cstddef>
#include <functional>

namespace car_terminal {

class MonitoringService {
public:
    using AcquireFunction = std::function<VehicleData()>;
    using ProcessFunction = std::function<void(const VehicleData&)>;

    MonitoringService(std::size_t sampleCount,
                      std::chrono::milliseconds samplingInterval,
                      std::size_t queueCapacity,
                      AcquireFunction acquire,
                      ProcessFunction process);

    void run();

private:
    std::size_t sampleCount_;
    std::chrono::milliseconds samplingInterval_;
    std::size_t queueCapacity_;
    AcquireFunction acquire_;
    ProcessFunction process_;
};

}  // namespace car_terminal
