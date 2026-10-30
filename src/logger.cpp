#include "car_terminal/logger.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace car_terminal {
namespace {

const char* levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    }
    return "UNKNOWN";
}

std::string timestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_r(&time, &localTime);

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

void createParentDirectory(const std::filesystem::path& filePath) {
    const auto parent = filePath.parent_path();
    if (parent.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        throw std::runtime_error("Cannot create log directory '" +
                                 parent.string() + "': " + error.message());
    }
}

std::uintmax_t regularFileSize(const std::filesystem::path& filePath) {
    std::error_code error;
    const bool regular = std::filesystem::is_regular_file(filePath, error);
    if (error) {
        throw std::runtime_error("Cannot inspect log file '" +
                                 filePath.string() + "': " + error.message());
    }
    if (!regular) {
        return 0;
    }
    const auto size = std::filesystem::file_size(filePath, error);
    if (error) {
        throw std::runtime_error("Cannot determine log file size '" +
                                 filePath.string() + "': " + error.message());
    }
    return size;
}

}  // namespace

Logger::Logger(const std::filesystem::path& filePath, std::ostream& console)
    : Logger(filePath, console, {}) {}

Logger::Logger(const std::filesystem::path& filePath,
               std::ostream& console,
               const FileRotationPolicy rotationPolicy)
    : filePath_(filePath),
      console_(console),
      rotationPolicy_(rotationPolicy) {
    validateRotationPolicy(rotationPolicy_, "Log");
    createParentDirectory(filePath_);
    file_.open(filePath_, std::ios::out | std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open log file: " +
                                 filePath_.string());
    }
    currentSize_ = regularFileSize(filePath_);
}

void Logger::rotateIfNeeded(const std::size_t pendingBytes) {
    if (!shouldRotateFile(currentSize_, pendingBytes, rotationPolicy_)) {
        return;
    }

    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to flush log file before rotation: " +
                                 filePath_.string());
    }
    file_.close();
    if (file_.fail()) {
        throw std::runtime_error("Failed to close log file for rotation: " +
                                 filePath_.string());
    }
    rotateFile(filePath_, rotationPolicy_);
    file_.clear();
    file_.open(filePath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot create log file after rotation: " +
                                 filePath_.string());
    }
    currentSize_ = 0;
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string line = "[" + timestampNow() + "] [" +
                             levelName(level) + "] " + message;
    rotateIfNeeded(line.size() + 1);
    file_ << line << '\n';
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to write log file: " +
                                 filePath_.string());
    }
    currentSize_ += line.size() + 1;
    console_ << line << '\n';
}

const std::filesystem::path& Logger::filePath() const noexcept {
    return filePath_;
}

}  // namespace car_terminal
