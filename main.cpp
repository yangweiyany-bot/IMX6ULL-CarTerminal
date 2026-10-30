#include "car_terminal/can_parser.h"
#include "car_terminal/cli.h"
#include "car_terminal/configuration.h"
#include "car_terminal/csv_recorder.h"
#include "car_terminal/logger.h"
#include "car_terminal/monitoring_service.h"
#include "car_terminal/sample_processor.h"
#include "car_terminal/simulator.h"
#include "car_terminal/signal_handler.h"
#include "car_terminal/socket_can_receiver.h"
#include "car_terminal/socket_can_source.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int runApplication(const std::filesystem::path& configPath,
                   const car_terminal::SignalHandler& signals) {
    const car_terminal::AppConfig config =
        car_terminal::loadConfig(configPath);
    car_terminal::Logger logger(config.logFile, std::cout);

    try {
        car_terminal::CsvRecorder recorder(config.csvFile);
        car_terminal::VehicleSimulator simulator;
        std::unique_ptr<car_terminal::SocketCanReceiver> canReceiver;
        std::unique_ptr<car_terminal::CanVehicleDataParser> canParser;
        std::unique_ptr<car_terminal::SocketCanDataSource> canSource;
        car_terminal::MonitoringService::AcquireFunction acquire;
        car_terminal::MonitoringService::CancelFunction cancelAcquisition;
        std::string sourceDescription;

        if (config.dataSource == car_terminal::DataSourceKind::Simulator) {
            acquire = [&simulator] { return simulator.nextSample(); };
            sourceDescription = "deterministic simulator";
        } else {
            if (!config.socketCan) {
                throw std::logic_error(
                    "SocketCAN source selected without SocketCAN configuration");
            }
            const auto& canConfig = *config.socketCan;
            canReceiver = std::make_unique<car_terminal::SocketCanReceiver>(
                canConfig.interfaceName, canConfig.filters);
            canParser =
                std::make_unique<car_terminal::CanVehicleDataParser>(
                    canConfig.mappings);
            canSource = std::make_unique<car_terminal::SocketCanDataSource>(
                *canReceiver, *canParser, canConfig.receiveTimeout,
                [&signals] { return signals.stopRequested(); });
            acquire = [&canSource] { return canSource->nextSample(); };
            cancelAcquisition = [&canSource] { canSource->requestStop(); };
            sourceDescription = "SocketCAN interface " +
                                canConfig.interfaceName;
        }

        car_terminal::SampleProcessor processor(
            config, logger, recorder, std::cout);

        std::cout << "===== Smart Car Terminal V"
                  << car_terminal::applicationVersion() << " =====\n";
        std::cout << "Data source: " << sourceDescription << '\n';
        if (config.dataSource == car_terminal::DataSourceKind::Simulator) {
            std::cout << "Simulation only: generated samples are not real "
                         "vehicle data.\n";
        } else {
            std::cout << "SocketCAN mappings are user supplied; this project "
                         "does not verify any vehicle protocol.\n";
        }
        std::cout << "Alarm thresholds are demonstration parameters, not real "
                     "vehicle diagnostic standards.\n";
        std::cout << "Configuration: " << configPath << "\n\n";
        logger.log(car_terminal::LogLevel::Info,
                   "Monitoring started using " + sourceDescription + " with " +
                       std::to_string(config.sampleCount) + " samples");

        car_terminal::MonitoringService monitoring(
            config.sampleCount,
            config.samplingInterval,
            config.queueCapacity,
            std::move(acquire),
            [&processor](const car_terminal::VehicleData& data) {
                static_cast<void>(processor.process(data));
            },
            std::move(cancelAcquisition));
        monitoring.run([&signals] { return signals.stopRequested(); });

        const int stopSignal = signals.signalNumber();
        if (stopSignal == 0) {
            logger.log(car_terminal::LogLevel::Info,
                       "Monitoring completed successfully");
        } else {
            logger.log(car_terminal::LogLevel::Warning,
                       "Monitoring stopped by signal " +
                           std::to_string(stopSignal) +
                           " after draining queued samples");
        }
        std::cout << "CSV data: " << recorder.filePath() << '\n';
        std::cout << "Log file: " << logger.filePath() << '\n';
        return stopSignal == 0 ? 0 : 128 + stopSignal;
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
        const car_terminal::SignalHandler signals;
        return runApplication(options.configPath, signals);
    } catch (const car_terminal::ConfigError& error) {
        std::cerr << "Configuration error: " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
}
