#include "car_terminal/csv_recorder.h"
#include "car_terminal/logger.h"
#include "test_support.h"

#include <chrono>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<std::filesystem::path> rotationFiles(
    const std::filesystem::path& active,
    const std::size_t maximumGeneration) {
    std::vector<std::filesystem::path> files;
    if (std::filesystem::exists(active)) {
        files.push_back(active);
    }
    for (std::size_t generation = 1; generation <= maximumGeneration;
         ++generation) {
        const auto path = car_terminal::rotatedFilePath(active, generation);
        if (std::filesystem::exists(path)) {
            files.push_back(path);
        }
    }
    return files;
}

std::size_t lineCount(const std::string& content) {
    return static_cast<std::size_t>(
        std::count(content.begin(), content.end(), '\n'));
}

}  // namespace

void runIoTests(test_support::TestSuite& suite) {
    using namespace test_support;

    suite.run("logger writes INFO WARNING and ERROR", [] {
        TempDirectory temp;
        const auto path = temp.path() / "logs/app.log";
        std::ostringstream console;
        {
            car_terminal::Logger logger(path, console);
            logger.log(car_terminal::LogLevel::Info, "information");
            logger.log(car_terminal::LogLevel::Warning, "warning");
            logger.log(car_terminal::LogLevel::Error, "error");
        }

        const std::string file = readText(path);
        require(file.find("[INFO] information") != std::string::npos,
                "INFO log entry missing");
        require(file.find("[WARNING] warning") != std::string::npos,
                "WARNING log entry missing");
        require(file.find("[ERROR] error") != std::string::npos,
                "ERROR log entry missing");
        require(console.str().find("[ERROR] error") != std::string::npos,
                "console log entry missing");

        std::ofstream append(path, std::ios::app);
        require(append.is_open(),
                "log file should be reusable after logger destruction");
    });

    suite.run("CSV recorder writes header data and statuses", [] {
        TempDirectory temp;
        const auto path = temp.path() / "records/data.csv";
        {
            car_terminal::CsvRecorder recorder(path);
            const auto timestamp = std::chrono::system_clock::from_time_t(0);
            recorder.record({1, timestamp, 80.0, 35.0, 12.8},
                            {false, false, false});
            recorder.record({2, timestamp, 130.0, 45.0, 12.0},
                            {true, true, true});
        }

        const std::string csv = readText(path);
        require(csv.find("sample,timestamp_utc,speed_kmh") == 0,
                "CSV header missing");
        require(csv.find("1,1970-01-01T00:00:00Z,80.00,35.00,12.80,NORMAL") !=
                    std::string::npos,
                "normal CSV row missing");
        require(csv.find(
                    "LOW_VOLTAGE|HIGH_TEMPERATURE|OVERSPEED") !=
                    std::string::npos,
                "warning CSV status missing");

        std::ofstream append(path, std::ios::app);
        require(append.is_open(),
                "CSV file should be reusable after recorder destruction");
    });

    suite.run("logger rotates repeatedly and enforces retention", [] {
        TempDirectory temp;
        const auto path = temp.path() / "rotation/app.log";
        std::ostringstream console;
        {
            car_terminal::Logger logger(path, console, {240, 2});
            for (int line = 0; line < 40; ++line) {
                logger.log(car_terminal::LogLevel::Info,
                           "rotation-line=" + std::to_string(line) +
                               " payload=abcdefghijklmnopqrstuvwxyz");
            }
        }

        require(std::filesystem::exists(path),
                "active log should be recreated after rotation");
        require(std::filesystem::exists(
                    car_terminal::rotatedFilePath(path, 1)),
                "newest log archive should exist");
        require(std::filesystem::exists(
                    car_terminal::rotatedFilePath(path, 2)),
                "second log archive should exist");
        require(!std::filesystem::exists(
                    car_terminal::rotatedFilePath(path, 3)),
                "archives beyond retention must not exist");
        require(readText(path).find("rotation-line=39") != std::string::npos,
                "active log should contain the newest record");
        require(readText(car_terminal::rotatedFilePath(path, 2))
                        .find("rotation-line=0") == std::string::npos,
                "oldest records should be removed after retention is exceeded");
        for (const auto& file : rotationFiles(path, 2)) {
            const auto content = readText(file);
            require(!content.empty(), "rotated log file should not be empty");
            std::istringstream input(content);
            std::string line;
            while (std::getline(input, line)) {
                require(!line.empty() && line.front() == '[' &&
                            line.find("] [INFO] ") != std::string::npos,
                        "each retained log record should remain a full line");
            }
        }
    });

    suite.run("CSV rotation recreates headers without losing rows", [] {
        TempDirectory temp;
        const auto path = temp.path() / "rotation/data.csv";
        constexpr std::size_t kRows = 18;
        {
            car_terminal::CsvRecorder recorder(path, {230, 20});
            for (std::size_t sample = 1; sample <= kRows; ++sample) {
                recorder.record({sample, {}, 80.0, 35.0, 12.8},
                                {false, false, false});
            }
        }

        const auto files = rotationFiles(path, 20);
        require(files.size() > std::size_t{1},
                "CSV should rotate more than once at a small threshold");
        std::size_t rows = 0;
        for (const auto& file : files) {
            std::istringstream input(readText(file));
            std::string line;
            require(static_cast<bool>(std::getline(input, line)),
                    "each CSV file should contain a header");
            requireEqual(
                line,
                std::string(
                    "sample,timestamp_utc,speed_kmh,temperature_c,voltage_v,status"),
                "each rotated CSV file should start with the standard header");
            while (std::getline(input, line)) {
                ++rows;
                requireEqual(std::count(line.begin(), line.end(), ','),
                             std::ptrdiff_t{5},
                             "each rotated CSV row should be complete");
            }
        }
        requireEqual(rows, kRows,
                     "rotation must preserve every recorded CSV row");
    });

    suite.run("logger reports file-open failure", [] {
        TempDirectory temp;
        std::ostringstream console;
        requireThrows<std::runtime_error>(
            [&] { car_terminal::Logger logger(temp.path(), console); },
            "Cannot open log file");
    });

    suite.run("CSV recorder reports file-open failure", [] {
        TempDirectory temp;
        requireThrows<std::runtime_error>(
            [&] { car_terminal::CsvRecorder recorder(temp.path()); },
            "Cannot open CSV file");
    });

    suite.run("logger serializes concurrent writers across rotations", [] {
        TempDirectory temp;
        const auto path = temp.path() / "concurrent.log";
        std::ostringstream console;
        {
            car_terminal::Logger logger(path, console, {500, 100});
            std::vector<std::thread> writers;
            for (int thread = 0; thread < 8; ++thread) {
                writers.emplace_back([&, thread] {
                    for (int line = 0; line < 25; ++line) {
                        logger.log(car_terminal::LogLevel::Info,
                                   "thread=" + std::to_string(thread) +
                                       " line=" + std::to_string(line));
                    }
                });
            }
            for (auto& writer : writers) {
                writer.join();
            }
        }
        std::size_t lines = 0;
        for (const auto& file : rotationFiles(path, 100)) {
            lines += lineCount(readText(file));
        }
        requireEqual(lines, std::size_t{200},
                     "concurrent rotated log line count");
    });

    suite.run("CSV recorder preserves concurrent rows across rotations", [] {
        TempDirectory temp;
        const auto path = temp.path() / "concurrent.csv";
        {
            car_terminal::CsvRecorder recorder(path, {320, 100});
            std::vector<std::thread> writers;
            for (std::size_t thread = 0; thread < 4; ++thread) {
                writers.emplace_back([&, thread] {
                    for (std::size_t row = 0; row < 25; ++row) {
                        const std::size_t sample = thread * 25 + row + 1;
                        recorder.record(
                            {sample, {}, 80.0, 35.0, 12.8},
                            {false, false, false});
                    }
                });
            }
            for (auto& writer : writers) {
                writer.join();
            }
        }

        std::size_t rows = 0;
        for (const auto& file : rotationFiles(path, 100)) {
            std::istringstream input(readText(file));
            std::string line;
            require(static_cast<bool>(std::getline(input, line)),
                    "rotated CSV should have a header");
            require(line.find("sample,timestamp_utc") == 0,
                    "rotated CSV header missing");
            while (std::getline(input, line)) {
                ++rows;
                requireEqual(std::count(line.begin(), line.end(), ','),
                             std::ptrdiff_t{5},
                             "each CSV row should contain six fields");
            }
        }
        requireEqual(rows, std::size_t{100},
                     "rotated CSV should preserve all 100 rows");
    });

    suite.run("log rotation reports archive removal failure", [] {
        TempDirectory temp;
        const auto path = temp.path() / "failure/app.log";
        const auto archive = car_terminal::rotatedFilePath(path, 1);
        std::filesystem::create_directories(archive);
        writeText(archive / "blocker", "not removable as one file");
        std::ostringstream console;
        car_terminal::Logger logger(path, console, {1, 1});
        logger.log(car_terminal::LogLevel::Info, "first line");
        requireThrows<std::runtime_error>(
            [&] { logger.log(car_terminal::LogLevel::Info, "second line"); },
            "Cannot remove oldest rotated file");
    });

    suite.run("CSV rotation reports archive removal failure", [] {
        TempDirectory temp;
        const auto path = temp.path() / "failure/data.csv";
        const auto archive = car_terminal::rotatedFilePath(path, 1);
        std::filesystem::create_directories(archive);
        writeText(archive / "blocker", "not removable as one file");
        car_terminal::CsvRecorder recorder(path, {1, 1});
        recorder.record({1, {}, 80.0, 35.0, 12.8},
                        {false, false, false});
        requireThrows<std::runtime_error>(
            [&] {
                recorder.record({2, {}, 80.0, 35.0, 12.8},
                                {false, false, false});
            },
            "Cannot remove oldest rotated file");
    });

    suite.run("log rotation reports rename failure", [] {
        TempDirectory temp;
        const auto directory = temp.path() / "rename-failure";
        const auto path = directory / "app.log";
        std::ostringstream console;
        car_terminal::Logger logger(path, console, {1, 1});
        logger.log(car_terminal::LogLevel::Info, "first line");
        std::filesystem::permissions(
            directory,
            std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_exec,
            std::filesystem::perm_options::replace);
        bool reportedRenameFailure = false;
        try {
            logger.log(car_terminal::LogLevel::Info, "second line");
        } catch (const std::runtime_error& error) {
            reportedRenameFailure =
                std::string(error.what()).find("Cannot rotate file") !=
                std::string::npos;
        }
        std::filesystem::permissions(
            directory, std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace);
        require(reportedRenameFailure,
                "rename failure should include a clear rotation error");
    });

    suite.run("logger reports write failure", [] {
        std::ostringstream console;
        car_terminal::Logger logger("/dev/full", console);
        requireThrows<std::runtime_error>(
            [&] { logger.log(car_terminal::LogLevel::Error, "cannot persist"); },
            "Failed to write log file");
    });

    suite.run("CSV recorder reports header write failure", [] {
        requireThrows<std::runtime_error>(
            [] { car_terminal::CsvRecorder recorder("/dev/full"); },
            "Failed to write CSV header");
    });
}
