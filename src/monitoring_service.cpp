#include "car_terminal/monitoring_service.h"

#include "car_terminal/blocking_queue.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace car_terminal {

MonitoringService::MonitoringService(
    std::size_t sampleCount,
    std::chrono::milliseconds samplingInterval,
    std::size_t queueCapacity,
    AcquireFunction acquire,
    ProcessFunction process,
    CancelFunction cancelAcquisition)
    : sampleCount_(sampleCount),
      samplingInterval_(samplingInterval),
      queueCapacity_(queueCapacity),
      acquire_(std::move(acquire)),
      process_(std::move(process)),
      cancelAcquisition_(std::move(cancelAcquisition)) {
    if (sampleCount_ == 0) {
        throw std::invalid_argument("Monitoring sample count must be positive");
    }
    if (samplingInterval_.count() < 0) {
        throw std::invalid_argument("Sampling interval must not be negative");
    }
    if (queueCapacity_ == 0) {
        throw std::invalid_argument("Monitoring queue capacity must be positive");
    }
    if (!acquire_ || !process_) {
        throw std::invalid_argument(
            "Monitoring acquire and process callbacks are required");
    }
}

void MonitoringService::run(StopFunction externalStopRequested) {
    BlockingQueue<VehicleData> queue(queueCapacity_);
    std::atomic<bool> internalStopRequested{false};
    const auto shouldStop = [&] {
        return internalStopRequested.load() ||
               (externalStopRequested && externalStopRequested());
    };

    std::mutex intervalMutex;
    std::condition_variable intervalCondition;
    std::mutex failureMutex;
    std::exception_ptr failure;

    const auto requestStop = [&] {
        internalStopRequested.store(true);
        if (cancelAcquisition_) {
            try {
                cancelAcquisition_();
            } catch (...) {
                // Cancellation must remain best-effort so queue closure and
                // thread joining still occur during another failure.
            }
        }
        queue.close();
        intervalCondition.notify_all();
    };

    const auto captureFailure = [&] {
        {
            std::lock_guard<std::mutex> lock(failureMutex);
            if (!failure) {
                failure = std::current_exception();
            }
        }
        requestStop();
    };

    const auto producer = [&] {
        try {
            for (std::size_t index = 0; index < sampleCount_; ++index) {
                if (shouldStop()) {
                    break;
                }
                if (!queue.push(acquire_())) {
                    break;
                }
                // A stop request that arrives during acquisition must not
                // discard the sample that has already been acquired.
                if (shouldStop()) {
                    break;
                }
                if (index + 1 < sampleCount_) {
                    std::unique_lock<std::mutex> lock(intervalMutex);
                    const auto deadline =
                        std::chrono::steady_clock::now() + samplingInterval_;
                    constexpr auto kStopPollInterval =
                        std::chrono::milliseconds(10);
                    while (!shouldStop()) {
                        const auto wakeTime = std::min(
                            deadline,
                            std::chrono::steady_clock::now() +
                                kStopPollInterval);
                        if (intervalCondition.wait_until(lock, wakeTime) ==
                                std::cv_status::timeout &&
                            std::chrono::steady_clock::now() >= deadline) {
                            break;
                        }
                    }
                }
            }
            queue.close();
        } catch (const AcquisitionStopped&) {
            queue.close();
        } catch (...) {
            captureFailure();
        }
    };

    const auto consumer = [&] {
        try {
            while (const auto value = queue.pop()) {
                process_(*value);
            }
        } catch (...) {
            captureFailure();
        }
    };

    std::thread producerThread;
    std::thread consumerThread;
    try {
        producerThread = std::thread(producer);
        consumerThread = std::thread(consumer);
    } catch (...) {
        requestStop();
        if (producerThread.joinable()) {
            producerThread.join();
        }
        if (consumerThread.joinable()) {
            consumerThread.join();
        }
        throw;
    }

    producerThread.join();
    consumerThread.join();

    std::exception_ptr capturedFailure;
    {
        std::lock_guard<std::mutex> lock(failureMutex);
        capturedFailure = failure;
    }
    if (capturedFailure) {
        std::rethrow_exception(capturedFailure);
    }
}

}  // namespace car_terminal
