#pragma once

#include "car_terminal/vehicle_data.h"

#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>

namespace car_terminal {

class AcquisitionStopped : public std::exception {
public:
    const char* what() const noexcept override {
        return "data acquisition stopped";
    }
};

class MonitoringService {
public:
    using AcquireFunction = std::function<VehicleData()>;
    using ProcessFunction = std::function<void(const VehicleData&)>;
    using StopFunction = std::function<bool()>;
    using CancelFunction = std::function<void()>;

    MonitoringService(std::size_t sampleCount,
                      std::chrono::milliseconds samplingInterval,
                      std::size_t queueCapacity,
                      AcquireFunction acquire,
                      ProcessFunction process,
                      CancelFunction cancelAcquisition = {});

    void run(StopFunction externalStopRequested = {});

private:
    std::size_t sampleCount_;
    std::chrono::milliseconds samplingInterval_;
    std::size_t queueCapacity_;
    AcquireFunction acquire_;
    ProcessFunction process_;
    CancelFunction cancelAcquisition_;
};

}  // namespace car_terminal
