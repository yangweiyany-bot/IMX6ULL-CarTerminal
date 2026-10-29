#include "car_terminal/sample_processor.h"

#include "car_terminal/status_checker.h"

#include <iomanip>
#include <sstream>

namespace car_terminal {

SampleProcessor::SampleProcessor(const AppConfig& config,
                                 Logger& logger,
                                 CsvRecorder& recorder,
                                 std::ostream& console)
    : config_(config),
      logger_(logger),
      recorder_(recorder),
      console_(console) {}

VehicleStatus SampleProcessor::process(const VehicleData& data) {
    const VehicleStatus status =
        checkVehicleStatus(data, config_.thresholds);

    console_ << std::fixed << std::setprecision(2);
    console_ << "----- Monitoring Sample " << data.sampleNumber << '/'
             << config_.sampleCount << " -----\n";
    console_ << "Speed: " << data.speedKmh << " km/h\n";
    console_ << "Temperature: " << data.temperatureC << " C\n";
    console_ << "Voltage: " << data.voltageV << " V\n";
    console_ << "Status: " << statusLabel(status) << '\n';

    for (const auto& warning :
         warningMessages(data, status, config_.thresholds)) {
        console_ << "WARNING: " << warning << '\n';
    }
    console_ << '\n';

    std::ostringstream summary;
    summary << std::fixed << std::setprecision(2)
            << "Sample " << data.sampleNumber
            << " status=" << statusLabel(status)
            << " speed=" << data.speedKmh << "km/h"
            << " temperature=" << data.temperatureC << "C"
            << " voltage=" << data.voltageV << "V";
    logger_.log(isNormal(status) ? LogLevel::Info : LogLevel::Warning,
                summary.str());
    recorder_.record(data, status);
    return status;
}

}  // namespace car_terminal
