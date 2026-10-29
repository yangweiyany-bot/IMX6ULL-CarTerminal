#pragma once

#include "car_terminal/configuration.h"
#include "car_terminal/vehicle_data.h"

#include <string>
#include <vector>

namespace car_terminal {

VehicleStatus checkVehicleStatus(const VehicleData& data,
                                 const AlarmThresholds& thresholds);
bool isNormal(const VehicleStatus& status);
std::string statusLabel(const VehicleStatus& status);
std::vector<std::string> warningMessages(
    const VehicleData& data,
    const VehicleStatus& status,
    const AlarmThresholds& thresholds);

}  // namespace car_terminal
