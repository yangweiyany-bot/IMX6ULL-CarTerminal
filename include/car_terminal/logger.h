#pragma once

#include "car_terminal/file_rotation.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <ostream>
#include <string>

namespace car_terminal {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

class Logger {
public:
    explicit Logger(const std::filesystem::path& filePath,
                    std::ostream& console);
    Logger(const std::filesystem::path& filePath,
           std::ostream& console,
           FileRotationPolicy rotationPolicy);

    void log(LogLevel level, const std::string& message);
    const std::filesystem::path& filePath() const noexcept;

private:
    void rotateIfNeeded(std::size_t pendingBytes);

    std::filesystem::path filePath_;
    std::ofstream file_;
    std::ostream& console_;
    FileRotationPolicy rotationPolicy_;
    std::uintmax_t currentSize_{};
    std::mutex mutex_;
};

}  // namespace car_terminal
