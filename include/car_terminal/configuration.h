#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <stdexcept>

namespace car_terminal {

struct AlarmThresholds {
    double lowVoltageV{};
    double highTemperatureC{};
    double overspeedKmh{};
};

struct AppConfig {
    AlarmThresholds thresholds;
    std::size_t sampleCount{};
    std::chrono::milliseconds samplingInterval{0};
    std::size_t queueCapacity{};
    std::filesystem::path logFile;
    std::filesystem::path csvFile;
};

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

AppConfig loadConfig(const std::filesystem::path& configPath);

}  // namespace car_terminal
