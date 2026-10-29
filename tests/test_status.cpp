#include "car_terminal/status_checker.h"
#include "test_support.h"

namespace {

const car_terminal::AlarmThresholds kThresholds{12.55, 40.0, 120.0};

car_terminal::VehicleData data(double speed,
                               double temperature,
                               double voltage) {
    return {1, {}, speed, temperature, voltage};
}

}  // namespace

void runStatusTests(test_support::TestSuite& suite) {
    using namespace test_support;

    suite.run("normal vehicle status", [] {
        const auto status = car_terminal::checkVehicleStatus(
            data(80.0, 35.0, 12.80), kThresholds);
        require(car_terminal::isNormal(status), "status should be normal");
        requireEqual(car_terminal::statusLabel(status),
                     std::string("NORMAL"),
                     "normal label");
    });

    suite.run("low-voltage status", [] {
        const auto status = car_terminal::checkVehicleStatus(
            data(80.0, 35.0, 12.54), kThresholds);
        require(status.lowVoltage, "low voltage should trigger");
        require(!status.highTemperature && !status.overspeed,
                "only low voltage should trigger");
    });

    suite.run("high-temperature status", [] {
        const auto status = car_terminal::checkVehicleStatus(
            data(80.0, 40.01, 12.80), kThresholds);
        require(status.highTemperature, "high temperature should trigger");
        require(!status.lowVoltage && !status.overspeed,
                "only high temperature should trigger");
    });

    suite.run("overspeed status", [] {
        const auto status = car_terminal::checkVehicleStatus(
            data(120.01, 35.0, 12.80), kThresholds);
        require(status.overspeed, "overspeed should trigger");
        require(!status.lowVoltage && !status.highTemperature,
                "only overspeed should trigger");
    });

    suite.run("threshold boundary rules", [] {
        const auto status = car_terminal::checkVehicleStatus(
            data(120.0, 40.0, 12.55), kThresholds);
        require(!status.lowVoltage,
                "voltage equal to threshold is not below threshold");
        require(status.highTemperature,
                "temperature equal to high threshold triggers");
        require(!status.overspeed,
                "speed equal to threshold is not over threshold");
    });

    suite.run("all warning messages", [] {
        const auto sample = data(130.0, 45.0, 12.0);
        const auto status =
            car_terminal::checkVehicleStatus(sample, kThresholds);
        const auto messages = car_terminal::warningMessages(
            sample, status, kThresholds);
        requireEqual(messages.size(), std::size_t{3},
                     "three warnings expected");
        require(messages[0].find("LOW VOLTAGE") != std::string::npos,
                "low-voltage message missing");
        require(messages[1].find("HIGH TEMPERATURE") != std::string::npos,
                "high-temperature message missing");
        require(messages[2].find("OVERSPEED") != std::string::npos,
                "overspeed message missing");
    });
}
