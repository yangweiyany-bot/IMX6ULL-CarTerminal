#include "car_terminal/monitoring_service.h"
#include "car_terminal/signal_handler.h"
#include "test_support.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <vector>

void runSignalTests(test_support::TestSuite& suite) {
    using namespace test_support;

    suite.run("signal handler records SIGINT without doing shutdown work", [] {
        const car_terminal::SignalHandler signals;
        requireEqual(std::raise(SIGINT), 0, "raising SIGINT should succeed");
        require(signals.stopRequested(), "SIGINT should request a stop");
        requireEqual(signals.signalNumber(), SIGINT,
                     "the first received signal should be retained");
    });

    suite.run("SIGTERM stops acquisition and drains acquired samples", [] {
        const car_terminal::SignalHandler signals;
        std::atomic<std::size_t> acquired{0};
        std::vector<std::size_t> processed;

        car_terminal::MonitoringService service(
            100,
            std::chrono::milliseconds(1),
            1,
            [&] {
                const std::size_t sample = ++acquired;
                if (sample == 3) {
                    requireEqual(std::raise(SIGTERM), 0,
                                 "raising SIGTERM should succeed");
                }
                return car_terminal::VehicleData{sample, {}, 80.0, 35.0,
                                                 12.8};
            },
            [&](const car_terminal::VehicleData& data) {
                processed.push_back(data.sampleNumber);
            });

        service.run([&signals] { return signals.stopRequested(); });

        requireEqual(signals.signalNumber(), SIGTERM,
                     "SIGTERM should be recorded");
        requireEqual(acquired.load(), std::size_t{3},
                     "no new sample should be acquired after SIGTERM");
        requireEqual(processed.size(), acquired.load(),
                     "every acquired sample must be drained");
        for (std::size_t index = 0; index < processed.size(); ++index) {
            requireEqual(processed[index], index + 1,
                         "drained samples should preserve their order");
        }
    });
}
