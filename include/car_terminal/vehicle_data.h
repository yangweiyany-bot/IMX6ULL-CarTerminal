#pragma once

#include <chrono>
#include <cstddef>

namespace car_terminal {

struct VehicleData {
    std::size_t sampleNumber{};
    std::chrono::system_clock::time_point timestamp{};
    double speedKmh{};
    double temperatureC{};
    double voltageV{};
};

struct VehicleStatus {
    bool lowVoltage{};
    bool highTemperature{};
    bool overspeed{};
};

}  // namespace car_terminal
