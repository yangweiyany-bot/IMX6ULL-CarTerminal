#include "car_terminal/cli.h"
#include "car_terminal/configuration.h"
#include "car_terminal/csv_recorder.h"
#include "car_terminal/logger.h"
#include "car_terminal/monitoring_service.h"
#include "car_terminal/sample_processor.h"
#include "car_terminal/simulator.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

int runApplication(const std::filesystem::path& configPath) {
    const car_terminal::AppConfig config =
        car_terminal::loadConfig(configPath);
    car_terminal::Logger logger(config.logFile, std::cout);

    try {
        car_terminal::CsvRecorder recorder(config.csvFile);
        car_terminal::VehicleSimulator simulator;
        car_terminal::SampleProcessor processor(
            config, logger, recorder, std::cout);

        std::cout << "===== Smart Car Terminal V"
                  << car_terminal::applicationVersion() << " =====\n";
        std::cout << "Simulation only: alarm thresholds are not real vehicle "
                     "diagnostic standards.\n";
        std::cout << "Configuration: " << configPath << "\n\n";
        logger.log(car_terminal::LogLevel::Info,
                   "Monitoring started with " +
                       std::to_string(config.sampleCount) + " samples");

        car_terminal::MonitoringService monitoring(
            config.sampleCount,
            config.samplingInterval,
            config.queueCapacity,
            [&simulator] { return simulator.nextSample(); },
            [&processor](const car_terminal::VehicleData& data) {
                static_cast<void>(processor.process(data));
            });
        monitoring.run();

        logger.log(car_terminal::LogLevel::Info,
                   "Monitoring completed successfully");
        std::cout << "CSV data: " << recorder.filePath() << '\n';
        std::cout << "Log file: " << logger.filePath() << '\n';
        return 0;
    } catch (const std::exception& error) {
        try {
            logger.log(car_terminal::LogLevel::Error, error.what());
        } catch (const std::exception&) {
        }
        throw;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> arguments;
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    car_terminal::CliOptions options;
    try {
        options = car_terminal::parseCommandLine(arguments);
    } catch (const car_terminal::CliError& error) {
        std::cerr << "Command-line error: " << error.what() << '\n'
                  << car_terminal::commandLineUsage(argv[0]);
        return 2;
    }

    if (options.action == car_terminal::CliAction::ShowHelp) {
        std::cout << car_terminal::commandLineUsage(argv[0]);
        return 0;
    }
    if (options.action == car_terminal::CliAction::ShowVersion) {
        std::cout << "IMX6ULL-CarTerminal "
                  << car_terminal::applicationVersion() << '\n';
        return 0;
    }

    try {
        return runApplication(options.configPath);
    } catch (const car_terminal::ConfigError& error) {
        std::cerr << "Configuration error: " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
}
