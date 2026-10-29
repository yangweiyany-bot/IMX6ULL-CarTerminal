#include "car_terminal/csv_recorder.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace car_terminal {
namespace {

void createParentDirectory(const std::filesystem::path& filePath) {
    const auto parent = filePath.parent_path();
    if (parent.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        throw std::runtime_error("Cannot create CSV directory '" +
                                 parent.string() + "': " + error.message());
    }
}

std::string formatTimestamp(
    const std::chrono::system_clock::time_point timestamp) {
    const std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
    std::tm utcTime{};
    gmtime_r(&time, &utcTime);

    std::ostringstream output;
    output << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string csvStatus(const VehicleStatus& status) {
    if (!status.lowVoltage && !status.highTemperature && !status.overspeed) {
        return "NORMAL";
    }

    std::string value;
    const auto append = [&value](const std::string& item) {
        if (!value.empty()) {
            value += '|';
        }
        value += item;
    };
    if (status.lowVoltage) {
        append("LOW_VOLTAGE");
    }
    if (status.highTemperature) {
        append("HIGH_TEMPERATURE");
    }
    if (status.overspeed) {
        append("OVERSPEED");
    }
    return value;
}

}  // namespace

CsvRecorder::CsvRecorder(const std::filesystem::path& filePath)
    : filePath_(filePath) {
    createParentDirectory(filePath_);
    file_.open(filePath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " +
                                 filePath_.string());
    }
    file_ << "sample,timestamp_utc,speed_kmh,temperature_c,voltage_v,status\n";
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to write CSV header: " +
                                 filePath_.string());
    }
}

void CsvRecorder::record(const VehicleData& data,
                         const VehicleStatus& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_ << data.sampleNumber << ',' << formatTimestamp(data.timestamp) << ','
          << std::fixed << std::setprecision(2) << data.speedKmh << ','
          << data.temperatureC << ',' << data.voltageV << ','
          << csvStatus(status) << '\n';
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to write CSV file: " +
                                 filePath_.string());
    }
}

const std::filesystem::path& CsvRecorder::filePath() const noexcept {
    return filePath_;
}

}  // namespace car_terminal
