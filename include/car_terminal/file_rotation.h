#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace car_terminal {

struct FileRotationPolicy {
    std::uintmax_t maxFileSizeBytes{};
    std::size_t retainedFileCount{};

    bool enabled() const noexcept;
};

void validateRotationPolicy(const FileRotationPolicy& policy,
                            const std::string& description);
bool shouldRotateFile(std::uintmax_t currentSize,
                      std::size_t pendingBytes,
                      const FileRotationPolicy& policy) noexcept;
std::filesystem::path rotatedFilePath(const std::filesystem::path& filePath,
                                      std::size_t generation);
void rotateFile(const std::filesystem::path& filePath,
                const FileRotationPolicy& policy);

}  // namespace car_terminal
