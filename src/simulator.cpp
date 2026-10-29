#include "car_terminal/simulator.h"

#include <chrono>

namespace car_terminal {

VehicleSimulator::VehicleSimulator() = default;

VehicleData VehicleSimulator::nextSample() {
    ++sampleNumber_;
    speedKmh_ += 7.50;
    temperatureC_ += 0.60;
    voltageV_ -= 0.03;

    return {
        sampleNumber_,
        std::chrono::system_clock::now(),
        speedKmh_,
        temperatureC_,
        voltageV_,
    };
}

}  // namespace car_terminal
