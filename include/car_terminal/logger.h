#pragma once

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

    void log(LogLevel level, const std::string& message);
    const std::filesystem::path& filePath() const noexcept;

private:
    std::filesystem::path filePath_;
    std::ofstream file_;
    std::ostream& console_;
    std::mutex mutex_;
};

}  // namespace car_terminal
