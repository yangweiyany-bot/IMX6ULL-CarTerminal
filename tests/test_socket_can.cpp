#include "car_terminal/can_parser.h"
#include "car_terminal/monitoring_service.h"
#include "car_terminal/socket_can_receiver.h"
#include "car_terminal/socket_can_source.h"
#include "test_support.h"

#include <chrono>
#include <atomic>
#include <cstddef>
#include <thread>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

car_terminal::RawCanFrame completeFrame();

class FakeCanReceiver final : public car_terminal::ICanFrameReceiver {
public:
    explicit FakeCanReceiver(
        std::vector<car_terminal::CanReceiveResult> results)
        : results_(std::move(results)) {}

    car_terminal::CanReceiveResult receive(
        std::chrono::milliseconds timeout,
        const StopFunction& stopRequested) override {
        lastTimeout_ = timeout;
        ++calls_;
        if (throwOnReceive_) {
            throw car_terminal::SocketCanError("injected receive failure");
        }
        if (stopRequested && stopRequested()) {
            return {car_terminal::CanReceiveStatus::Stopped, {}};
        }
        if (index_ >= results_.size()) {
            return {car_terminal::CanReceiveStatus::Timeout, {}};
        }
        return results_[index_++];
    }

    void setThrowOnReceive() {
        throwOnReceive_ = true;
    }

    std::size_t calls() const {
        return calls_;
    }

    std::chrono::milliseconds lastTimeout() const {
        return lastTimeout_;
    }

private:
    std::vector<car_terminal::CanReceiveResult> results_;
    std::size_t index_{};
    std::size_t calls_{};
    std::chrono::milliseconds lastTimeout_{0};
    bool throwOnReceive_{};
};

class CancellationAwareReceiver final
    : public car_terminal::ICanFrameReceiver {
public:
    car_terminal::CanReceiveResult receive(
        std::chrono::milliseconds,
        const StopFunction& stopRequested) override {
        if (calls_.fetch_add(1) == 0) {
            return {car_terminal::CanReceiveStatus::Frame, completeFrame()};
        }

        waiting_.store(true);
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < deadline) {
            if (stopRequested && stopRequested()) {
                cancellationObserved_.store(true);
                return {car_terminal::CanReceiveStatus::Stopped, {}};
            }
            std::this_thread::yield();
        }
        return {car_terminal::CanReceiveStatus::Timeout, {}};
    }

    bool waiting() const {
        return waiting_.load();
    }

    bool cancellationObserved() const {
        return cancellationObserved_.load();
    }

private:
    std::atomic<std::size_t> calls_{0};
    std::atomic<bool> waiting_{false};
    std::atomic<bool> cancellationObserved_{false};
};

car_terminal::RawCanFrame completeFrame() {
    car_terminal::RawCanFrame frame;
    frame.identifier = 0x123;
    frame.dataLength = 6;
    frame.data = {0xD2, 0x04, 0xFA, 0x00, 0x05, 0x00, 0, 0};
    return frame;
}

std::vector<car_terminal::CanSignalMapping> mappings() {
    using namespace car_terminal;
    return {
        {VehicleSignal::SpeedKmh, 0x123, CanFrameFormat::Standard,
         0, 2, CanByteOrder::LittleEndian, CanValueType::Unsigned,
         0.01, 0.0},
        {VehicleSignal::TemperatureC, 0x123, CanFrameFormat::Standard,
         2, 2, CanByteOrder::LittleEndian, CanValueType::Signed,
         0.1, 0.0},
        {VehicleSignal::VoltageV, 0x123, CanFrameFormat::Standard,
         4, 2, CanByteOrder::BigEndian, CanValueType::Unsigned,
         0.01, 0.0},
    };
}

}  // namespace

void runSocketCanTests(test_support::TestSuite& suite) {
    using namespace car_terminal;
    using namespace test_support;

    suite.run("SocketCAN source tolerates timeout before data", [] {
        FakeCanReceiver receiver({
            {CanReceiveStatus::Timeout, {}},
            {CanReceiveStatus::Timeout, {}},
            {CanReceiveStatus::Frame, completeFrame()},
        });
        CanVehicleDataParser parser(mappings());
        SocketCanDataSource source(receiver, parser,
                                   std::chrono::milliseconds(25),
                                   [] { return false; });
        const auto data = source.nextSample();
        requireEqual(receiver.calls(), std::size_t{3},
                     "timeouts should retry without fabricating data");
        requireEqual(receiver.lastTimeout().count(), 25LL,
                     "configured receive timeout should be forwarded");
        requireNear(data.speedKmh, 12.34, 1e-9, "decoded speed");
    });

    suite.run("SocketCAN source cooperates with stop request", [] {
        FakeCanReceiver receiver({});
        CanVehicleDataParser parser(mappings());
        SocketCanDataSource source(receiver, parser,
                                   std::chrono::milliseconds(10),
                                   [] { return true; });
        requireThrows<AcquisitionStopped>(
            [&] { static_cast<void>(source.nextSample()); },
            "acquisition stopped");
        requireEqual(receiver.calls(), std::size_t{1},
                     "receiver should observe stop immediately");
    });

    suite.run("SocketCAN timeout retries observe a later stop request", [] {
        FakeCanReceiver receiver({
            {CanReceiveStatus::Timeout, {}},
            {CanReceiveStatus::Timeout, {}},
            {CanReceiveStatus::Timeout, {}},
        });
        CanVehicleDataParser parser(mappings());
        SocketCanDataSource source(receiver, parser,
                                   std::chrono::milliseconds(10),
                                   [&receiver] {
                                       return receiver.calls() >= 3;
                                   });
        requireThrows<AcquisitionStopped>(
            [&] { static_cast<void>(source.nextSample()); },
            "acquisition stopped");
        requireEqual(receiver.calls(), std::size_t{3},
                     "timeout loop should re-check the stop callback before "
                     "waiting again");
    });

    suite.run("consumer failure cancels SocketCAN acquisition", [] {
        CancellationAwareReceiver receiver;
        CanVehicleDataParser parser(mappings());
        SocketCanDataSource source(receiver, parser,
                                   std::chrono::milliseconds(10),
                                   [] { return false; });
        MonitoringService service(
            RunMode::Continuous, 0, std::chrono::milliseconds(1), 1,
            [&source] { return source.nextSample(); },
            [&receiver](const VehicleData&) {
                const auto deadline = std::chrono::steady_clock::now() +
                                      std::chrono::seconds(1);
                while (!receiver.waiting() &&
                       std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::yield();
                }
                if (!receiver.waiting()) {
                    throw std::runtime_error(
                        "producer did not begin its next acquisition");
                }
                throw std::runtime_error("simulated consumer I/O failure");
            },
            [&source] { source.requestStop(); });

        requireThrows<std::runtime_error>(
            [&] { service.run(); }, "simulated consumer I/O failure");
        require(receiver.cancellationObserved(),
                "consumer failure should cancel an in-flight CAN wait");
    });

    suite.run("SocketCAN source propagates receive errors", [] {
        FakeCanReceiver receiver({});
        receiver.setThrowOnReceive();
        CanVehicleDataParser parser(mappings());
        SocketCanDataSource source(receiver, parser,
                                   std::chrono::milliseconds(10),
                                   [] { return false; });
        requireThrows<SocketCanError>(
            [&] { static_cast<void>(source.nextSample()); },
            "injected receive failure");
    });

    suite.run("SocketCAN source validates construction", [] {
        FakeCanReceiver receiver({});
        CanVehicleDataParser parser(mappings());
        requireThrows<std::invalid_argument>(
            [&] {
                SocketCanDataSource source(
                    receiver, parser, std::chrono::milliseconds(0),
                    [] { return false; });
            },
            "timeout");
        SocketCanDataSource::StopFunction missingStop;
        requireThrows<std::invalid_argument>(
            [&] {
                SocketCanDataSource source(
                    receiver, parser, std::chrono::milliseconds(10),
                    missingStop);
            },
            "stop callback");
    });

    suite.run("SocketCAN initialization rejects missing interface", [] {
        const std::vector<CanFilterRule> filters{
            {0x123, 0x7FF, CanFrameFormat::Standard},
        };
        requireThrows<SocketCanError>(
            [&] { SocketCanReceiver receiver("vcan_missing", filters); },
            "unavailable");
    });

    suite.run("SocketCAN initialization requires filters", [] {
        const std::vector<CanFilterRule> filters;
        requireThrows<SocketCanError>(
            [&] { SocketCanReceiver receiver("vcan0", filters); },
            "at least one CAN filter");
    });
}
