#include "car_terminal/monitoring_service.h"
#include "test_support.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <vector>

void runThreadingTests(test_support::TestSuite& suite) {
    using namespace test_support;

    suite.run("producer-consumer completes in order", [] {
        constexpr std::size_t kSamples = 25;
        std::atomic<std::size_t> produced{0};
        std::vector<std::size_t> processed;

        car_terminal::MonitoringService service(
            kSamples,
            std::chrono::milliseconds(1),
            2,
            [&] {
                const std::size_t number = ++produced;
                return car_terminal::VehicleData{number, {}, 80.0, 35.0,
                                                 12.8};
            },
            [&](const car_terminal::VehicleData& value) {
                processed.push_back(value.sampleNumber);
            });

        service.run();
        requireEqual(produced.load(), kSamples,
                     "producer should acquire every sample");
        requireEqual(processed.size(), kSamples,
                     "consumer should process every sample");
        for (std::size_t index = 0; index < kSamples; ++index) {
            requireEqual(processed[index], index + 1,
                         "queue should preserve sample order");
        }
    });

    suite.run("continuous monitoring runs until requested and drains queue", [] {
        std::atomic<bool> stopRequested{false};
        std::atomic<std::size_t> produced{0};
        std::vector<std::size_t> processed;

        car_terminal::MonitoringService service(
            car_terminal::RunMode::Continuous,
            0,
            std::chrono::milliseconds(1),
            2,
            [&] {
                const std::size_t number = ++produced;
                return car_terminal::VehicleData{number, {}, 80.0, 35.0,
                                                 12.8};
            },
            [&](const car_terminal::VehicleData& value) {
                processed.push_back(value.sampleNumber);
                if (processed.size() >= 25) {
                    stopRequested.store(true);
                }
            });

        service.run([&] { return stopRequested.load(); });
        require(processed.size() >= std::size_t{25},
                "continuous mode should not stop at a finite sample count");
        requireEqual(processed.size(), produced.load(),
                     "continuous shutdown should drain every acquired sample");
        for (std::size_t index = 0; index < processed.size(); ++index) {
            requireEqual(processed[index], index + 1,
                         "continuous queue should preserve sample order");
        }
    });

    suite.run("consumer failure stops and joins threads", [] {
        std::atomic<std::size_t> produced{0};
        std::atomic<std::size_t> processed{0};

        car_terminal::MonitoringService service(
            100,
            std::chrono::milliseconds(1),
            2,
            [&] {
                const std::size_t number = ++produced;
                return car_terminal::VehicleData{number, {}, 80.0, 35.0,
                                                 12.8};
            },
            [&](const car_terminal::VehicleData&) {
                const std::size_t count = ++processed;
                if (count == 3) {
                    throw std::runtime_error("simulated consumer failure");
                }
            });

        requireThrows<std::runtime_error>(
            [&] { service.run(); }, "simulated consumer failure");
        requireEqual(processed.load(), std::size_t{3},
                     "consumer should stop at injected failure");
        require(produced.load() < std::size_t{100},
                "producer should be interrupted after consumer failure");
    });

    suite.run("producer failure closes queue and joins threads", [] {
        std::atomic<std::size_t> attempts{0};
        std::atomic<std::size_t> processed{0};

        car_terminal::MonitoringService service(
            100,
            std::chrono::milliseconds(1),
            2,
            [&] {
                const std::size_t attempt = ++attempts;
                if (attempt == 4) {
                    throw std::runtime_error("simulated producer failure");
                }
                return car_terminal::VehicleData{attempt, {}, 80.0, 35.0,
                                                 12.8};
            },
            [&](const car_terminal::VehicleData&) { ++processed; });

        requireThrows<std::runtime_error>(
            [&] { service.run(); }, "simulated producer failure");
        requireEqual(attempts.load(), std::size_t{4},
                     "producer should stop at injected failure");
        requireEqual(processed.load(), std::size_t{3},
                     "consumer should drain samples queued before failure");
    });

    suite.run("continuous acquisition failure closes and joins threads", [] {
        std::atomic<std::size_t> attempts{0};
        std::atomic<std::size_t> processed{0};

        car_terminal::MonitoringService service(
            car_terminal::RunMode::Continuous,
            0,
            std::chrono::milliseconds(1),
            2,
            [&] {
                const std::size_t attempt = ++attempts;
                if (attempt == 4) {
                    throw std::runtime_error(
                        "continuous acquisition failure");
                }
                return car_terminal::VehicleData{attempt, {}, 80.0, 35.0,
                                                 12.8};
            },
            [&](const car_terminal::VehicleData&) { ++processed; });

        requireThrows<std::runtime_error>(
            [&] { service.run(); }, "continuous acquisition failure");
        requireEqual(attempts.load(), std::size_t{4},
                     "continuous producer should stop at failure");
        requireEqual(processed.load(), std::size_t{3},
                     "continuous consumer should drain queued samples");
    });

    suite.run("invalid monitoring arguments", [] {
        requireThrows<std::invalid_argument>(
            [] {
                car_terminal::MonitoringService service(
                    0,
                    std::chrono::milliseconds(1),
                    1,
                    [] { return car_terminal::VehicleData{}; },
                    [](const car_terminal::VehicleData&) {});
            },
            "sample count");

        requireThrows<std::invalid_argument>(
            [] {
                car_terminal::MonitoringService service(
                    1,
                    std::chrono::milliseconds(1),
                    0,
                    [] { return car_terminal::VehicleData{}; },
                    [](const car_terminal::VehicleData&) {});
            },
            "queue capacity");

        requireThrows<std::invalid_argument>(
            [] {
                car_terminal::MonitoringService service(
                    1,
                    std::chrono::milliseconds(-1),
                    1,
                    [] { return car_terminal::VehicleData{}; },
                    [](const car_terminal::VehicleData&) {});
            },
            "interval");

        requireThrows<std::invalid_argument>(
            [] {
                car_terminal::MonitoringService::AcquireFunction acquire;
                car_terminal::MonitoringService service(
                    1,
                    std::chrono::milliseconds(1),
                    1,
                    acquire,
                    [](const car_terminal::VehicleData&) {});
            },
            "callbacks are required");

        requireThrows<std::invalid_argument>(
            [] {
                car_terminal::MonitoringService service(
                    car_terminal::RunMode::Continuous,
                    0,
                    std::chrono::milliseconds(0),
                    1,
                    [] { return car_terminal::VehicleData{}; },
                    [](const car_terminal::VehicleData&) {});
            },
            "Continuous monitoring interval");
    });
}
