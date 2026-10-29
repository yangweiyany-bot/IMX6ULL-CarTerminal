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

    suite.run("logger serializes concurrent writers", [] {
        TempDirectory temp;
        const auto path = temp.path() / "concurrent.log";
        std::ostringstream console;
        {
            car_terminal::Logger logger(path, console);
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
        const auto content = readText(path);
        requireEqual(std::count(content.begin(), content.end(), '\n'),
                     std::ptrdiff_t{200},
                     "concurrent log line count");
    });

    suite.run("CSV recorder preserves concurrent rows", [] {
        TempDirectory temp;
        const auto path = temp.path() / "concurrent.csv";
        {
            car_terminal::CsvRecorder recorder(path);
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

        std::istringstream input(readText(path));
        std::string line;
        std::size_t lines = 0;
        while (std::getline(input, line)) {
            ++lines;
            requireEqual(std::count(line.begin(), line.end(), ','),
                         std::ptrdiff_t{5},
                         "each CSV row should contain six fields");
        }
        requireEqual(lines, std::size_t{101},
                     "CSV should contain header and 100 rows");
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
