#ifndef THREAD_SAFE_QUEUE_
#define THREAD_SAFE_QUEUE_

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T> class ThreadSafeQueue {
  public:
    explicit ThreadSafeQueue(size_t max_size = 8)
        : max_size_(max_size) {}

    // Disable copy constructor and assignment operator
    ThreadSafeQueue(const ThreadSafeQueue &) = delete;
    ThreadSafeQueue &operator=(const ThreadSafeQueue &) = delete;

    // Return false when the queue is full.
    bool push(T t) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.size() >= max_size_) {
                return false;
            }
            queue_.push(std::move(t));
        }
        cv_.notify_one();
        return true;
    }

    // blocking pop with timeout
    std::optional<T> pop(int timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto timeout = std::chrono::milliseconds(timeout_ms);
        bool notified = cv_.wait_for(lock, timeout, [this] {
            return !queue_.empty();
        });

        if (!notified || queue_.empty()) {
            return std::nullopt;
        }
        T t = std::move(queue_.front());
        queue_.pop();
        return t;
    }

    // non-blocking pop
    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T t = std::move(queue_.front());
        queue_.pop();
        return t;
    }

    std::optional<T> front() {
        std::lock_guard<std::mutex> lock(mutex_);
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
    std::condition_variable cv_;
    const size_t max_size_;
};

#endif
