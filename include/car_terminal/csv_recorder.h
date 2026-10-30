#pragma once

#include "car_terminal/file_rotation.h"
#include "car_terminal/vehicle_data.h"

#include <filesystem>
#include <fstream>
#include <mutex>

namespace car_terminal {

class CsvRecorder {
public:
    explicit CsvRecorder(const std::filesystem::path& filePath);
    CsvRecorder(const std::filesystem::path& filePath,
                FileRotationPolicy rotationPolicy);

    void record(const VehicleData& data, const VehicleStatus& status);
    const std::filesystem::path& filePath() const noexcept;

private:
    void openNewFile();
    void rotateIfNeeded(std::size_t pendingBytes);

    std::filesystem::path filePath_;
    std::ofstream file_;
    FileRotationPolicy rotationPolicy_;
    std::uintmax_t currentSize_{};
    std::mutex mutex_;
};

}  // namespace car_terminal
