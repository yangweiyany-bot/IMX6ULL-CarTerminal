#include "car_terminal/csv_recorder.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace car_terminal {
namespace {

constexpr const char* kCsvHeader =
    "sample,timestamp_utc,speed_kmh,temperature_c,voltage_v,status\n";

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
    : CsvRecorder(filePath, {}) {}

CsvRecorder::CsvRecorder(const std::filesystem::path& filePath,
                         const FileRotationPolicy rotationPolicy)
    : filePath_(filePath), rotationPolicy_(rotationPolicy) {
    validateRotationPolicy(rotationPolicy_, "CSV");
    createParentDirectory(filePath_);
    openNewFile();
}

void CsvRecorder::openNewFile() {
    file_.clear();
    file_.open(filePath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " +
                                 filePath_.string());
    }
    file_ << kCsvHeader;
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to write CSV header: " +
                                 filePath_.string());
    }
    currentSize_ = std::char_traits<char>::length(kCsvHeader);
}

void CsvRecorder::rotateIfNeeded(const std::size_t pendingBytes) {
    const auto headerSize = std::char_traits<char>::length(kCsvHeader);
    if (currentSize_ == headerSize ||
        !shouldRotateFile(currentSize_, pendingBytes, rotationPolicy_)) {
        return;
    }

    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to flush CSV file before rotation: " +
                                 filePath_.string());
    }
    file_.close();
    if (file_.fail()) {
        throw std::runtime_error("Failed to close CSV file for rotation: " +
                                 filePath_.string());
    }
    rotateFile(filePath_, rotationPolicy_);
    openNewFile();
}

void CsvRecorder::record(const VehicleData& data,
                         const VehicleStatus& status) {
    std::ostringstream output;
    output << data.sampleNumber << ',' << formatTimestamp(data.timestamp)
           << ',' << std::fixed << std::setprecision(2) << data.speedKmh << ','
           << data.temperatureC << ',' << data.voltageV << ','
           << csvStatus(status) << '\n';
    const std::string row = output.str();

    std::lock_guard<std::mutex> lock(mutex_);
    rotateIfNeeded(row.size());
    file_ << row;
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to write CSV file: " +
                                 filePath_.string());
    }
    currentSize_ += row.size();
}

const std::filesystem::path& CsvRecorder::filePath() const noexcept {
    return filePath_;
}

}  // namespace car_terminal
