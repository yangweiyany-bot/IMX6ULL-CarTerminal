#include "car_terminal/configuration.h"
#include "test_support.h"

#include <filesystem>
#include <string>

namespace {

const char* kValidConfig =
    "low_voltage_threshold_v=12.55\n"
    "high_temperature_threshold_c=40.0\n"
    "overspeed_threshold_kmh=120.0\n"
    "sample_count=10\n"
    "sampling_interval_ms=25\n"
    "queue_capacity=4\n"
    "log_file=output/app.log\n"
    "csv_file=output/data.csv\n";

void expectConfigError(const std::string& content,
                       const std::string& messageFragment) {
    test_support::TempDirectory temp;
    const auto path = temp.path() / "test.conf";
    test_support::writeText(path, content);
    test_support::requireThrows<car_terminal::ConfigError>(
        [&] { static_cast<void>(car_terminal::loadConfig(path)); },
        messageFragment);
}

}  // namespace

void runConfigurationTests(test_support::TestSuite& suite) {
    using namespace test_support;

    suite.run("valid configuration", [] {
        TempDirectory temp;
        const auto path = temp.path() / "valid.conf";
        writeText(path, kValidConfig);
        const auto config = car_terminal::loadConfig(path);

        requireNear(config.thresholds.lowVoltageV, 12.55, 1e-9,
                    "low-voltage threshold");
        requireNear(config.thresholds.highTemperatureC, 40.0, 1e-9,
                    "temperature threshold");
        requireNear(config.thresholds.overspeedKmh, 120.0, 1e-9,
                    "speed threshold");
        requireEqual(config.sampleCount, std::size_t{10}, "sample count");
        requireEqual(config.samplingInterval.count(), 25LL,
                     "sampling interval");
        requireEqual(config.queueCapacity, std::size_t{4},
                     "queue capacity");
        require(config.logFile == temp.path() / "output/app.log",
                "relative log path should resolve from config directory");
        require(config.csvFile == temp.path() / "output/data.csv",
                "relative CSV path should resolve from config directory");
    });

    suite.run("missing configuration file", [] {
        requireThrows<car_terminal::ConfigError>(
            [] {
                static_cast<void>(car_terminal::loadConfig(
                    "/definitely/missing/car-terminal.conf"));
            },
            "Cannot open configuration file");
    });

    suite.run("missing required key", [] {
        std::string content = kValidConfig;
        const auto start = content.find("csv_file=");
        content.erase(start);
        expectConfigError(content, "Missing required configuration key");
    });

    suite.run("malformed configuration line", [] {
        expectConfigError(std::string(kValidConfig) + "not-a-key-value\n",
                          "Expected key=value");
    });

    suite.run("unknown configuration key", [] {
        expectConfigError(std::string(kValidConfig) + "mystery=1\n",
                          "Unknown configuration key");
    });

    suite.run("duplicate configuration key", [] {
        expectConfigError(std::string(kValidConfig) + "sample_count=4\n",
                          "Duplicate configuration key");
    });

    suite.run("invalid numeric configuration", [] {
        std::string content = kValidConfig;
        const auto start = content.find("sample_count=10");
        content.replace(start, std::string("sample_count=10").size(),
                        "sample_count=ten");
        expectConfigError(content, "Invalid non-negative integer");
    });

    suite.run("unreasonable configuration value", [] {
        std::string content = kValidConfig;
        const auto start = content.find("sample_count=10");
        content.replace(start, std::string("sample_count=10").size(),
                        "sample_count=0");
        expectConfigError(content, "sample_count must be in");
    });

    suite.run("invalid queue capacity", [] {
        std::string content = kValidConfig;
        const auto start = content.find("queue_capacity=4");
        content.replace(start, std::string("queue_capacity=4").size(),
                        "queue_capacity=0");
        expectConfigError(content, "queue_capacity must be in");
    });

    suite.run("numeric overflow configuration", [] {
        std::string content = kValidConfig;
        const auto start = content.find("sample_count=10");
        content.replace(start, std::string("sample_count=10").size(),
                        "sample_count=999999999999999999999999999999");
        expectConfigError(content, "Invalid non-negative integer");
    });

    suite.run("non-finite threshold configuration", [] {
        std::string content = kValidConfig;
        const auto start = content.find("low_voltage_threshold_v=12.55");
        content.replace(start,
                        std::string("low_voltage_threshold_v=12.55").size(),
                        "low_voltage_threshold_v=nan");
        expectConfigError(content, "Invalid floating-point value");
    });

    suite.run("conflicting output paths", [] {
        std::string content = kValidConfig;
        const auto start = content.find("csv_file=output/data.csv");
        content.replace(start, std::string("csv_file=output/data.csv").size(),
                        "csv_file=output/app.log");
        expectConfigError(content, "must refer to different files");
    });

    suite.run("output path cannot overwrite configuration", [] {
        std::string content = kValidConfig;
        const auto start = content.find("csv_file=output/data.csv");
        content.replace(start, std::string("csv_file=output/data.csv").size(),
                        "csv_file=test.conf");
        expectConfigError(content,
                          "must not overwrite the configuration file");
    });
}
