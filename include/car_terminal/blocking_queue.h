#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>

namespace car_terminal {

template <typename Value>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("BlockingQueue capacity must be positive");
        }
    }

    bool push(Value value) {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [this] {
            return closed_ || values_.size() < capacity_;
        });
        if (closed_) {
            return false;
        }
        values_.push(std::move(value));
        lock.unlock();
        notEmpty_.notify_one();
        return true;
    }

    std::optional<Value> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [this] { return closed_ || !values_.empty(); });
        if (values_.empty()) {
            return std::nullopt;
        }
        Value value = std::move(values_.front());
        values_.pop();
        lock.unlock();
        notFull_.notify_one();
        return value;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

private:
    const std::size_t capacity_;
    std::queue<Value> values_;
    bool closed_{};
    std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
};

}  // namespace car_terminal
