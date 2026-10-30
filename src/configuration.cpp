#include "car_terminal/configuration.h"

#include "car_terminal/can_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace car_terminal {
namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

double parseDouble(const std::string& key, const std::string& value) {
    std::size_t parsed = 0;
    double result = 0.0;
    try {
        result = std::stod(value, &parsed);
    } catch (const std::exception&) {
        throw ConfigError("Invalid floating-point value for '" + key +
                          "': " + value);
    }
    if (parsed != value.size() || !std::isfinite(result)) {
        throw ConfigError("Invalid floating-point value for '" + key +
                          "': " + value);
    }
    return result;
}

std::size_t parseSize(const std::string& key, const std::string& value) {
    if (value.empty() || value.front() == '-') {
        throw ConfigError("Invalid non-negative integer for '" + key +
                          "': " + value);
    }

    std::size_t parsed = 0;
    unsigned long long result = 0;
    try {
        result = std::stoull(value, &parsed);
    } catch (const std::exception&) {
        throw ConfigError("Invalid non-negative integer for '" + key +
                          "': " + value);
    }
    if (parsed != value.size() ||
        result > std::numeric_limits<std::size_t>::max()) {
        throw ConfigError("Invalid non-negative integer for '" + key +
                          "': " + value);
    }
    return static_cast<std::size_t>(result);
}

std::uint32_t parseCanUnsigned(const std::string& key,
                               const std::string& value) {
    if (value.empty() || value.front() == '-') {
        throw ConfigError("Invalid CAN integer for '" + key + "': " +
                          value);
    }
    std::size_t parsed = 0;
    unsigned long long result = 0;
    try {
        result = std::stoull(value, &parsed, 0);
    } catch (const std::exception&) {
        throw ConfigError("Invalid CAN integer for '" + key + "': " +
                          value);
    }
    if (parsed != value.size() ||
        result > std::numeric_limits<std::uint32_t>::max()) {
        throw ConfigError("Invalid CAN integer for '" + key + "': " +
                          value);
    }
    return static_cast<std::uint32_t>(result);
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::size_t begin = 0;
    while (true) {
        const std::size_t end = value.find(delimiter, begin);
        parts.push_back(trim(value.substr(begin, end - begin)));
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return parts;
}

CanFrameFormat parseCanFormat(const std::string& key,
                              const std::string& value) {
    if (value == "standard") {
        return CanFrameFormat::Standard;
    }
    if (value == "extended") {
        return CanFrameFormat::Extended;
    }
    throw ConfigError("Invalid CAN frame format for '" + key +
                      "': expected standard or extended");
}

CanByteOrder parseCanByteOrder(const std::string& key,
                               const std::string& value) {
    if (value == "little") {
        return CanByteOrder::LittleEndian;
    }
    if (value == "big") {
        return CanByteOrder::BigEndian;
    }
    throw ConfigError("Invalid CAN byte order for '" + key +
                      "': expected little or big");
}

CanValueType parseCanValueType(const std::string& key,
                               const std::string& value) {
    if (value == "unsigned") {
        return CanValueType::Unsigned;
    }
    if (value == "signed") {
        return CanValueType::Signed;
    }
    throw ConfigError("Invalid CAN value type for '" + key +
                      "': expected unsigned or signed");
}

std::vector<CanFilterRule> parseCanFilters(const std::string& value) {
    if (value.empty()) {
        throw ConfigError("can_filters must not be empty");
    }

    std::vector<CanFilterRule> filters;
    std::set<std::tuple<int, std::uint32_t, std::uint32_t>> unique;
    for (const auto& specification : split(value, ',')) {
        const auto fields = split(specification, ':');
        if (fields.size() != 3) {
            throw ConfigError(
                "Each can_filters entry must be format:id:mask");
        }
        CanFilterRule filter;
        filter.format = parseCanFormat("can_filters", fields[0]);
        filter.identifier = parseCanUnsigned("can_filters", fields[1]);
        filter.mask = parseCanUnsigned("can_filters", fields[2]);
        try {
            validateCanFilter(filter);
        } catch (const CanParseError& error) {
            throw ConfigError(std::string("Invalid can_filters entry: ") +
                              error.what());
        }
        const auto identity = std::make_tuple(
            filter.format == CanFrameFormat::Standard ? 0 : 1,
            filter.identifier, filter.mask);
        if (!unique.insert(identity).second) {
            throw ConfigError("Duplicate can_filters entry: " +
                              specification);
        }
        filters.push_back(filter);
    }
    return filters;
}

CanSignalMapping parseCanMapping(const std::string& key,
                                 const std::string& value,
                                 VehicleSignal signal) {
    const auto fields = split(value, ':');
    if (fields.size() != 8) {
        throw ConfigError(
            key + " must be format:id:byte_offset:byte_length:" +
                  "byte_order:value_type:scale:value_offset");
    }

    CanSignalMapping mapping;
    mapping.signal = signal;
    mapping.format = parseCanFormat(key, fields[0]);
    mapping.identifier = parseCanUnsigned(key, fields[1]);
    mapping.byteOffset = parseSize(key, fields[2]);
    mapping.byteLength = parseSize(key, fields[3]);
    mapping.byteOrder = parseCanByteOrder(key, fields[4]);
    mapping.valueType = parseCanValueType(key, fields[5]);
    mapping.scale = parseDouble(key, fields[6]);
    mapping.valueOffset = parseDouble(key, fields[7]);
    try {
        validateCanSignalMapping(mapping);
    } catch (const CanParseError& error) {
        throw ConfigError("Invalid " + key + ": " + error.what());
    }
    return mapping;
}

void validateCanInterfaceName(const std::string& interfaceName) {
    if (interfaceName.empty()) {
        throw ConfigError("can_interface must not be empty");
    }
    if (interfaceName.size() >= 16) {
        throw ConfigError("can_interface must be shorter than 16 characters");
    }
    const bool valid = std::all_of(
        interfaceName.begin(), interfaceName.end(), [](unsigned char value) {
            return std::isalnum(value) != 0 || value == '_' || value == '-' ||
                   value == '.';
        });
    if (!valid) {
        throw ConfigError("can_interface contains unsupported characters");
    }
}

std::string requireValue(
    const std::unordered_map<std::string, std::string>& values,
    const std::string& key) {
    const auto found = values.find(key);
    if (found == values.end()) {
        throw ConfigError("Missing required configuration key: " + key);
    }
    if (found->second.empty()) {
        throw ConfigError("Configuration value must not be empty: " + key);
    }
    return found->second;
}

bool pathsReferToSameFile(const std::filesystem::path& left,
                          const std::filesystem::path& right) {
    if (left == right) {
        return true;
    }
    std::error_code error;
    const bool equivalent = std::filesystem::equivalent(left, right, error);
    return !error && equivalent;
}

void validateConfig(const AppConfig& config) {
    constexpr std::uintmax_t kMaximumManagedFileSize =
        std::uintmax_t{1} << 40;
    if (config.thresholds.lowVoltageV <= 0.0 ||
        config.thresholds.lowVoltageV > 100.0) {
        throw ConfigError("low_voltage_threshold_v must be in (0, 100]");
    }
    if (config.thresholds.highTemperatureC < -100.0 ||
        config.thresholds.highTemperatureC > 300.0) {
        throw ConfigError(
            "high_temperature_threshold_c must be in [-100, 300]");
    }
    if (config.thresholds.overspeedKmh <= 0.0 ||
        config.thresholds.overspeedKmh > 1000.0) {
        throw ConfigError("overspeed_threshold_kmh must be in (0, 1000]");
    }
    if (config.sampleCount == 0 || config.sampleCount > 1000000) {
        throw ConfigError("sample_count must be in [1, 1000000]");
    }
    if (config.samplingInterval.count() <= 0 ||
        config.samplingInterval.count() > 3600000) {
        throw ConfigError("sampling_interval_ms must be in [1, 3600000]");
    }
    if (config.queueCapacity == 0 || config.queueCapacity > 10000) {
        throw ConfigError("queue_capacity must be in [1, 10000]");
    }
    if (config.logMaxSizeBytes == 0 ||
        config.logMaxSizeBytes > kMaximumManagedFileSize) {
        throw ConfigError(
            "log_max_size_bytes must be in [1, 1099511627776]");
    }
    if (config.csvMaxSizeBytes == 0 ||
        config.csvMaxSizeBytes > kMaximumManagedFileSize) {
        throw ConfigError(
            "csv_max_size_bytes must be in [1, 1099511627776]");
    }
    if (config.rotatedFileCount == 0 ||
        config.rotatedFileCount > 100) {
        throw ConfigError("rotated_file_count must be in [1, 100]");
    }
}

}  // namespace

AppConfig loadConfig(const std::filesystem::path& configPath) {
    std::ifstream input(configPath);
    if (!input.is_open()) {
        throw ConfigError("Cannot open configuration file: " +
                          configPath.string());
    }

    const std::unordered_set<std::string> allowedKeys{
        "low_voltage_threshold_v",
        "high_temperature_threshold_c",
        "overspeed_threshold_kmh",
        "run_mode",
        "sample_count",
        "sampling_interval_ms",
        "queue_capacity",
        "log_max_size_bytes",
        "csv_max_size_bytes",
        "rotated_file_count",
        "log_file",
        "csv_file",
        "data_source",
        "can_interface",
        "can_receive_timeout_ms",
        "can_filters",
        "can_speed_mapping",
        "can_temperature_mapping",
        "can_voltage_mapping",
    };
    std::unordered_map<std::string, std::string> values;

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw ConfigError("Expected key=value at line " +
                              std::to_string(lineNumber));
        }
        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (key.empty()) {
            throw ConfigError("Empty configuration key at line " +
                              std::to_string(lineNumber));
        }
        if (allowedKeys.count(key) == 0) {
            throw ConfigError("Unknown configuration key '" + key +
                              "' at line " + std::to_string(lineNumber));
        }
        if (!values.emplace(key, value).second) {
            throw ConfigError("Duplicate configuration key '" + key +
                              "' at line " + std::to_string(lineNumber));
        }
    }
    if (input.bad()) {
        throw ConfigError("Failed while reading configuration file: " +
                          configPath.string());
    }

    AppConfig config;
    config.thresholds.lowVoltageV = parseDouble(
        "low_voltage_threshold_v",
        requireValue(values, "low_voltage_threshold_v"));
    config.thresholds.highTemperatureC = parseDouble(
        "high_temperature_threshold_c",
        requireValue(values, "high_temperature_threshold_c"));
    config.thresholds.overspeedKmh = parseDouble(
        "overspeed_threshold_kmh",
        requireValue(values, "overspeed_threshold_kmh"));
    const auto runMode = values.find("run_mode");
    const std::string runModeValue =
        runMode == values.end() ? "finite" : runMode->second;
    if (runModeValue == "finite") {
        config.runMode = RunMode::Finite;
    } else if (runModeValue == "continuous") {
        config.runMode = RunMode::Continuous;
    } else {
        throw ConfigError("run_mode must be either finite or continuous");
    }
    config.sampleCount =
        parseSize("sample_count", requireValue(values, "sample_count"));
    const auto interval = parseSize(
        "sampling_interval_ms",
        requireValue(values, "sampling_interval_ms"));
    if (interval >
        static_cast<std::size_t>(std::numeric_limits<long long>::max())) {
        throw ConfigError("sampling_interval_ms is too large");
    }
    config.samplingInterval =
        std::chrono::milliseconds(static_cast<long long>(interval));
    config.queueCapacity = parseSize(
        "queue_capacity", requireValue(values, "queue_capacity"));

    const auto optionalSize = [&values](const std::string& key,
                                        const std::size_t fallback) {
        const auto entry = values.find(key);
        return entry == values.end() ? fallback
                                     : parseSize(key, entry->second);
    };
    config.logMaxSizeBytes = optionalSize(
        "log_max_size_bytes",
        static_cast<std::size_t>(kDefaultLogMaxSizeBytes));
    config.csvMaxSizeBytes = optionalSize(
        "csv_max_size_bytes",
        static_cast<std::size_t>(kDefaultCsvMaxSizeBytes));
    config.rotatedFileCount = optionalSize(
        "rotated_file_count", kDefaultRotatedFileCount);

    const auto dataSource = values.find("data_source");
    const std::string dataSourceValue =
        dataSource == values.end() ? "simulator" : dataSource->second;
    if (dataSourceValue == "simulator") {
        config.dataSource = DataSourceKind::Simulator;
    } else if (dataSourceValue == "socketcan") {
        config.dataSource = DataSourceKind::SocketCan;
    } else {
        throw ConfigError(
            "data_source must be either simulator or socketcan");
    }

    const std::array<const char*, 6> canKeys{
        "can_interface",
        "can_receive_timeout_ms",
        "can_filters",
        "can_speed_mapping",
        "can_temperature_mapping",
        "can_voltage_mapping",
    };
    if (config.dataSource == DataSourceKind::Simulator) {
        for (const char* key : canKeys) {
            if (values.count(key) != 0) {
                throw ConfigError(std::string(key) +
                                  " requires data_source=socketcan");
            }
        }
    } else {
        SocketCanConfig canConfig;
        canConfig.interfaceName = requireValue(values, "can_interface");
        validateCanInterfaceName(canConfig.interfaceName);

        const auto receiveTimeout = parseSize(
            "can_receive_timeout_ms",
            requireValue(values, "can_receive_timeout_ms"));
        if (receiveTimeout == 0 || receiveTimeout > 60000) {
            throw ConfigError(
                "can_receive_timeout_ms must be in [1, 60000]");
        }
        canConfig.receiveTimeout = std::chrono::milliseconds(
            static_cast<long long>(receiveTimeout));
        canConfig.filters =
            parseCanFilters(requireValue(values, "can_filters"));
        canConfig.mappings = {
            parseCanMapping("can_speed_mapping",
                            requireValue(values, "can_speed_mapping"),
                            VehicleSignal::SpeedKmh),
            parseCanMapping("can_temperature_mapping",
                            requireValue(values, "can_temperature_mapping"),
                            VehicleSignal::TemperatureC),
            parseCanMapping("can_voltage_mapping",
                            requireValue(values, "can_voltage_mapping"),
                            VehicleSignal::VoltageV),
        };
        for (const auto& mapping : canConfig.mappings) {
            const bool accepted = std::any_of(
                canConfig.filters.begin(), canConfig.filters.end(),
                [&mapping](const CanFilterRule& filter) {
                    return filter.format == mapping.format &&
                           (mapping.identifier & filter.mask) ==
                               (filter.identifier & filter.mask);
                });
            if (!accepted) {
                throw ConfigError(
                    "CAN signal mapping identifier is not accepted by "
                    "can_filters");
            }
        }
        config.socketCan = std::move(canConfig);
    }

    const auto baseDirectory =
        std::filesystem::absolute(configPath).parent_path();
    config.logFile = requireValue(values, "log_file");
    config.csvFile = requireValue(values, "csv_file");
    if (config.logFile.is_relative()) {
        config.logFile = (baseDirectory / config.logFile).lexically_normal();
    }
    if (config.csvFile.is_relative()) {
        config.csvFile = (baseDirectory / config.csvFile).lexically_normal();
    }
    config.logFile = config.logFile.lexically_normal();
    config.csvFile = config.csvFile.lexically_normal();

    if (pathsReferToSameFile(config.logFile, config.csvFile)) {
        throw ConfigError("log_file and csv_file must refer to different files");
    }

    const auto normalizedConfigPath =
        std::filesystem::absolute(configPath).lexically_normal();
    if (pathsReferToSameFile(config.logFile, normalizedConfigPath) ||
        pathsReferToSameFile(config.csvFile, normalizedConfigPath)) {
        throw ConfigError("output files must not overwrite the configuration file");
    }

    validateConfig(config);
    return config;
}

}  // namespace car_terminal
