#include "car_terminal/file_rotation.h"

#include <stdexcept>
#include <system_error>

namespace car_terminal {
namespace {

bool pathExists(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw std::runtime_error("Cannot inspect rotation path '" +
                                 path.string() + "': " + error.message());
    }
    return exists;
}

void renamePath(const std::filesystem::path& source,
                const std::filesystem::path& destination) {
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    if (error) {
        throw std::runtime_error("Cannot rotate file '" + source.string() +
                                 "' to '" + destination.string() + "': " +
                                 error.message());
    }
}

}  // namespace

bool FileRotationPolicy::enabled() const noexcept {
    return maxFileSizeBytes != 0 || retainedFileCount != 0;
}

void validateRotationPolicy(const FileRotationPolicy& policy,
                            const std::string& description) {
    if (!policy.enabled()) {
        return;
    }
    if (policy.maxFileSizeBytes == 0 || policy.retainedFileCount == 0) {
        throw std::invalid_argument(
            description +
            " rotation requires a positive size and retained file count");
    }
}

bool shouldRotateFile(const std::uintmax_t currentSize,
                      const std::size_t pendingBytes,
                      const FileRotationPolicy& policy) noexcept {
    if (!policy.enabled() || currentSize == 0) {
        return false;
    }
    if (currentSize >= policy.maxFileSizeBytes) {
        return true;
    }
    return pendingBytes > policy.maxFileSizeBytes - currentSize;
}

std::filesystem::path rotatedFilePath(
    const std::filesystem::path& filePath,
    const std::size_t generation) {
    return std::filesystem::path(filePath.string() + "." +
                                 std::to_string(generation));
}

void rotateFile(const std::filesystem::path& filePath,
                const FileRotationPolicy& policy) {
    validateRotationPolicy(policy, "File");
    if (!policy.enabled()) {
        return;
    }

    const auto oldest =
        rotatedFilePath(filePath, policy.retainedFileCount);
    if (pathExists(oldest)) {
        std::error_code error;
        if (!std::filesystem::remove(oldest, error) || error) {
            throw std::runtime_error("Cannot remove oldest rotated file '" +
                                     oldest.string() + "': " +
                                     (error ? error.message()
                                            : "path was not removed"));
        }
    }

    for (std::size_t generation = policy.retainedFileCount;
         generation > 1;
         --generation) {
        const auto source = rotatedFilePath(filePath, generation - 1);
        if (pathExists(source)) {
            renamePath(source, rotatedFilePath(filePath, generation));
        }
    }

    if (pathExists(filePath)) {
        renamePath(filePath, rotatedFilePath(filePath, 1));
    }
}

}  // namespace car_terminal
