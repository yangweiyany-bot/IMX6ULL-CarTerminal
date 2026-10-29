#pragma once

#include "car_terminal/vehicle_data.h"

#include <filesystem>
#include <fstream>
#include <mutex>

namespace car_terminal {

class CsvRecorder {
public:
    explicit CsvRecorder(const std::filesystem::path& filePath);

    void record(const VehicleData& data, const VehicleStatus& status);
    const std::filesystem::path& filePath() const noexcept;

private:
    std::filesystem::path filePath_;
    std::ofstream file_;
    std::mutex mutex_;
};

}  // namespace car_terminal
