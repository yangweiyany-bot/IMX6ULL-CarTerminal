#pragma once

#include "car_terminal/vehicle_data.h"

namespace car_terminal {

class VehicleSimulator {
public:
    VehicleSimulator();
    VehicleData nextSample();

private:
    std::size_t sampleNumber_{};
    double speedKmh_{60.50};
    double temperatureC_{35.20};
    double voltageV_{12.71};
};

}  // namespace car_terminal
