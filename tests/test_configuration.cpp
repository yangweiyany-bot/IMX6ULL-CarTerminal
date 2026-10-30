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

std::string validSocketCanConfig() {
    return std::string(kValidConfig) +
           "data_source=socketcan\n"
           "can_interface=vcan0\n"
           "can_receive_timeout_ms=50\n"
           "can_filters=standard:0x123:0x7FF,"
           "extended:0x18FF50E5:0x1FFFFFFF\n"
           "can_speed_mapping=standard:0x123:0:2:little:unsigned:0.01:0\n"
           "can_temperature_mapping=standard:0x123:2:2:little:signed:0.1:0\n"
           "can_voltage_mapping=standard:0x123:4:2:big:unsigned:0.01:0\n";
}

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
        require(config.dataSource == car_terminal::DataSourceKind::Simulator,
                "omitted data source should preserve simulator compatibility");
        require(config.runMode == car_terminal::RunMode::Finite,
                "omitted run mode should preserve finite compatibility");
        require(!config.socketCan.has_value(),
                "simulator configuration should not create CAN settings");
    });

    suite.run("continuous run mode configuration", [] {
        TempDirectory temp;
        const auto path = temp.path() / "continuous.conf";
        writeText(path, std::string(kValidConfig) +
                            "run_mode=continuous\n");
        const auto config = car_terminal::loadConfig(path);
        require(config.runMode == car_terminal::RunMode::Continuous,
                "continuous run mode should be selected");
        requireEqual(config.sampleCount, std::size_t{10},
                     "sample count remains parsed for compatibility");
    });

    suite.run("valid SocketCAN configuration", [] {
        TempDirectory temp;
        const auto path = temp.path() / "socketcan.conf";
        writeText(path, validSocketCanConfig());
        const auto config = car_terminal::loadConfig(path);
        require(config.dataSource == car_terminal::DataSourceKind::SocketCan,
                "SocketCAN source selection");
        require(config.socketCan.has_value(), "SocketCAN settings required");
        requireEqual(config.socketCan->interfaceName, std::string("vcan0"),
                     "CAN interface");
        requireEqual(config.socketCan->receiveTimeout.count(), 50LL,
                     "CAN receive timeout");
        requireEqual(config.socketCan->filters.size(), std::size_t{2},
                     "CAN filter count");
        requireEqual(config.socketCan->mappings.size(), std::size_t{3},
                     "CAN mapping count");
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

    suite.run("invalid data source is rejected", [] {
        expectConfigError(std::string(kValidConfig) +
                              "data_source=automatic\n",
                          "data_source must be");
    });

    suite.run("invalid run mode is rejected", [] {
        expectConfigError(std::string(kValidConfig) +
                              "run_mode=forever\n",
                          "run_mode must be either finite or continuous");
    });

    suite.run("simulator rejects stray CAN configuration", [] {
        expectConfigError(std::string(kValidConfig) +
                              "can_interface=vcan0\n",
                          "requires data_source=socketcan");
    });

    suite.run("SocketCAN configuration requires every mapping", [] {
        std::string content = validSocketCanConfig();
        const auto begin = content.find("can_voltage_mapping=");
        content.erase(begin);
        expectConfigError(content, "can_voltage_mapping");
    });

    suite.run("SocketCAN invalid standard identifier is rejected", [] {
        std::string content = validSocketCanConfig();
        const auto begin = content.find(
            "can_filters=standard:0x123:0x7FF,");
        content.replace(begin,
                        std::string("can_filters=standard:0x123:0x7FF,")
                            .size(),
                        "can_filters=standard:0x800:0x7FF,");
        expectConfigError(content, "filter identifier");
    });

    suite.run("SocketCAN duplicate filters are rejected", [] {
        std::string content = validSocketCanConfig();
        const auto begin = content.find(
            "can_filters=standard:0x123:0x7FF,"
            "extended:0x18FF50E5:0x1FFFFFFF");
        content.replace(
            begin,
            std::string("can_filters=standard:0x123:0x7FF,"
                        "extended:0x18FF50E5:0x1FFFFFFF")
                .size(),
            "can_filters=standard:0x123:0x7FF,"
            "standard:0x123:0x7FF");
        expectConfigError(content, "Duplicate can_filters");
    });

    suite.run("SocketCAN mapping payload overflow is rejected", [] {
        std::string content = validSocketCanConfig();
        const auto begin = content.find(
            "can_speed_mapping=standard:0x123:0:2");
        content.replace(
            begin,
            std::string("can_speed_mapping=standard:0x123:0:2").size(),
            "can_speed_mapping=standard:0x123:7:2");
        expectConfigError(content, "exceeds the classic CAN payload");
    });

    suite.run("SocketCAN mappings must be accepted by filters", [] {
        std::string content = validSocketCanConfig();
        const auto begin = content.find(
            "can_voltage_mapping=standard:0x123");
        content.replace(
            begin,
            std::string("can_voltage_mapping=standard:0x123").size(),
            "can_voltage_mapping=standard:0x124");
        expectConfigError(content, "not accepted by can_filters");
    });

    suite.run("SocketCAN timeout and interface are validated", [] {
        std::string content = validSocketCanConfig();
        auto begin = content.find("can_receive_timeout_ms=50");
        content.replace(begin,
                        std::string("can_receive_timeout_ms=50").size(),
                        "can_receive_timeout_ms=0");
        expectConfigError(content, "must be in [1, 60000]");

        content = validSocketCanConfig();
        begin = content.find("can_interface=vcan0");
        content.replace(begin, std::string("can_interface=vcan0").size(),
                        "can_interface=bad interface");
        expectConfigError(content, "unsupported characters");
    });
}
