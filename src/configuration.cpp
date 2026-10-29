#include "car_terminal/configuration.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

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
        "sample_count",
        "sampling_interval_ms",
        "queue_capacity",
        "log_file",
        "csv_file",
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
