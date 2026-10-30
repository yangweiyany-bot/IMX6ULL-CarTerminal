#pragma once

#include "car_terminal/can_types.h"
#include "car_terminal/run_mode.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace car_terminal {

inline constexpr std::uintmax_t kDefaultLogMaxSizeBytes = 10U * 1024U * 1024U;
inline constexpr std::uintmax_t kDefaultCsvMaxSizeBytes = 50U * 1024U * 1024U;
inline constexpr std::size_t kDefaultRotatedFileCount = 5;

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
    RunMode runMode{RunMode::Finite};
    std::size_t sampleCount{};
    std::chrono::milliseconds samplingInterval{0};
    std::size_t queueCapacity{};
    std::filesystem::path logFile;
    std::filesystem::path csvFile;
    std::uintmax_t logMaxSizeBytes{kDefaultLogMaxSizeBytes};
    std::uintmax_t csvMaxSizeBytes{kDefaultCsvMaxSizeBytes};
    std::size_t rotatedFileCount{kDefaultRotatedFileCount};
    DataSourceKind dataSource{DataSourceKind::Simulator};
    std::optional<SocketCanConfig> socketCan;
};

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

AppConfig loadConfig(const std::filesystem::path& configPath);

}  // namespace car_terminal
