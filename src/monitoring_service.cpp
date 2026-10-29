#include "car_terminal/monitoring_service.h"

#include "car_terminal/blocking_queue.h"

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
    ProcessFunction process)
    : sampleCount_(sampleCount),
      samplingInterval_(samplingInterval),
      queueCapacity_(queueCapacity),
      acquire_(std::move(acquire)),
      process_(std::move(process)) {
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

void MonitoringService::run() {
    BlockingQueue<VehicleData> queue(queueCapacity_);
    std::atomic<bool> stopRequested{false};
    std::mutex intervalMutex;
    std::condition_variable intervalCondition;
    std::mutex failureMutex;
    std::exception_ptr failure;

    const auto requestStop = [&] {
        stopRequested.store(true);
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
                if (stopRequested.load()) {
                    break;
                }
                if (!queue.push(acquire_())) {
                    break;
                }
                if (index + 1 < sampleCount_) {
                    std::unique_lock<std::mutex> lock(intervalMutex);
                    intervalCondition.wait_for(
                        lock,
                        samplingInterval_,
                        [&] { return stopRequested.load(); });
                }
            }
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
