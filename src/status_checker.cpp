#include "car_terminal/status_checker.h"

#include <iomanip>
#include <sstream>

namespace car_terminal {

VehicleStatus checkVehicleStatus(const VehicleData& data,
                                 const AlarmThresholds& thresholds) {
    return {
        data.voltageV < thresholds.lowVoltageV,
        data.temperatureC >= thresholds.highTemperatureC,
        data.speedKmh > thresholds.overspeedKmh,
    };
}

bool isNormal(const VehicleStatus& status) {
    return !status.lowVoltage && !status.highTemperature && !status.overspeed;
}

std::string statusLabel(const VehicleStatus& status) {
    return isNormal(status) ? "NORMAL" : "WARNING";
}

std::vector<std::string> warningMessages(
    const VehicleData& data,
    const VehicleStatus& status,
    const AlarmThresholds& thresholds) {
    std::vector<std::string> messages;
    std::ostringstream text;
    text << std::fixed << std::setprecision(2);

    if (status.lowVoltage) {
        text << "LOW VOLTAGE: " << data.voltageV << " V < "
             << thresholds.lowVoltageV << " V";
        messages.push_back(text.str());
        text.str({});
        text.clear();
    }
    if (status.highTemperature) {
        text << "HIGH TEMPERATURE: " << data.temperatureC << " C >= "
             << thresholds.highTemperatureC << " C";
        messages.push_back(text.str());
        text.str({});
        text.clear();
    }
    if (status.overspeed) {
        text << "OVERSPEED: " << data.speedKmh << " km/h > "
             << thresholds.overspeedKmh << " km/h";
        messages.push_back(text.str());
    }
    return messages;
}

}  // namespace car_terminal
