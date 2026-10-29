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

}  // namespace

Logger::Logger(const std::filesystem::path& filePath, std::ostream& console)
    : filePath_(filePath), console_(console) {
    createParentDirectory(filePath_);
    file_.open(filePath_, std::ios::out | std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open log file: " +
                                 filePath_.string());
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string line = "[" + timestampNow() + "] [" +
                             levelName(level) + "] " + message;
    console_ << line << '\n';
    file_ << line << '\n';
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed to write log file: " +
                                 filePath_.string());
    }
}

const std::filesystem::path& Logger::filePath() const noexcept {
    return filePath_;
}

}  // namespace car_terminal
