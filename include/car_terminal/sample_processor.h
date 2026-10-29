#pragma once

#include "car_terminal/configuration.h"
#include "car_terminal/csv_recorder.h"
#include "car_terminal/logger.h"
#include "car_terminal/vehicle_data.h"

#include <ostream>

namespace car_terminal {

class SampleProcessor {
public:
    SampleProcessor(const AppConfig& config,
                    Logger& logger,
                    CsvRecorder& recorder,
                    std::ostream& console);

    VehicleStatus process(const VehicleData& data);

private:
    const AppConfig& config_;
    Logger& logger_;
    CsvRecorder& recorder_;
    std::ostream& console_;
};

}  // namespace car_terminal
