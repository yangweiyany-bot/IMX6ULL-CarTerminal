#include "car_terminal/blocking_queue.h"
#include "test_support.h"

#include <chrono>
#include <future>
#include <optional>
#include <thread>

void runBlockingQueueTests(test_support::TestSuite& suite) {
    using namespace test_support;
    using namespace std::chrono_literals;

    suite.run("blocking queue drains values after close", [] {
        car_terminal::BlockingQueue<int> queue(2);
        require(queue.push(10), "first push should succeed");
        require(queue.push(20), "second push should succeed");
        queue.close();
        require(!queue.push(30), "push after close should fail");
        requireEqual(queue.pop().value_or(-1), 10, "first queued value");
        requireEqual(queue.pop().value_or(-1), 20, "second queued value");
        require(!queue.pop().has_value(),
                "closed empty queue should end consumption");
    });

    suite.run("blocking queue close releases blocked producer", [] {
        car_terminal::BlockingQueue<int> queue(1);
        require(queue.push(1), "initial push should fill queue");
        std::promise<void> started;
        std::promise<bool> result;
        auto resultFuture = result.get_future();

        std::thread producer([&] {
            started.set_value();
            result.set_value(queue.push(2));
        });
        started.get_future().wait();
        const bool wasBlocked = resultFuture.wait_for(20ms) ==
                                std::future_status::timeout;
        queue.close();
        const bool pushResult = resultFuture.get();
        producer.join();

        require(wasBlocked, "producer should block while queue is full");
        require(!pushResult, "close should make blocked push return false");
        requireEqual(queue.pop().value_or(-1), 1,
                     "previously queued value should remain available");
    });

    suite.run("blocking queue close releases blocked consumer", [] {
        car_terminal::BlockingQueue<int> queue(1);
        std::promise<void> started;
        std::promise<std::optional<int>> result;
        auto resultFuture = result.get_future();

        std::thread consumer([&] {
            started.set_value();
            result.set_value(queue.pop());
        });
        started.get_future().wait();
        const bool wasBlocked = resultFuture.wait_for(20ms) ==
                                std::future_status::timeout;
        queue.close();
        const auto popResult = resultFuture.get();
        consumer.join();

        require(wasBlocked, "consumer should block while queue is empty");
        require(!popResult.has_value(),
                "close should end a blocked empty pop");
    });

    suite.run("blocking queue rejects zero capacity", [] {
        requireThrows<std::invalid_argument>(
            [] { car_terminal::BlockingQueue<int> queue(0); },
            "capacity must be positive");
    });
}
