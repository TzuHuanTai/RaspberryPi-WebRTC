#ifndef THREAD_SAFE_QUEUE_
#define THREAD_SAFE_QUEUE_

#include <mutex>
#include <optional>
#include <queue>

template <typename T> class ThreadSafeQueue {
  public:
    explicit ThreadSafeQueue(size_t max_size = 8)
        : max_size_(max_size) {}

    // Return false when the queue is full.
    bool push(T t) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) {
            return false;
        }
        queue_.push(t);
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T t = queue_.front();
        queue_.pop();
        return t;
    }

    std::optional<T> front() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T t = queue_.front();
        return t;
    }

    bool full() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size() >= max_size_;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_ = std::queue<T>();
    }

  private:
    std::queue<T> queue_;
    std::mutex mutex_;
    size_t max_size_;
};

#endif
