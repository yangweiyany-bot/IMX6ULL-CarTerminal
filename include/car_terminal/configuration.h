#pragma once

#include "car_terminal/can_types.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace car_terminal {

struct AlarmThresholds {
    double lowVoltageV{};
    double highTemperatureC{};
    double overspeedKmh{};
};

enum class DataSourceKind {
    Simulator,
    SocketCan,
};

struct SocketCanConfig {
    std::string interfaceName;
    std::chrono::milliseconds receiveTimeout{0};
    std::vector<CanFilterRule> filters;
    std::vector<CanSignalMapping> mappings;
};

struct AppConfig {
    AlarmThresholds thresholds;
    std::size_t sampleCount{};
    std::chrono::milliseconds samplingInterval{0};
    std::size_t queueCapacity{};
    std::filesystem::path logFile;
    std::filesystem::path csvFile;
    DataSourceKind dataSource{DataSourceKind::Simulator};
    std::optional<SocketCanConfig> socketCan;
};

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

AppConfig loadConfig(const std::filesystem::path& configPath);

}  // namespace car_terminal
