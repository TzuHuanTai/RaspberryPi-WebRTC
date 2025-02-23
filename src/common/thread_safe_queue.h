#ifndef THREAD_SAFE_QUEUE_
#define THREAD_SAFE_QUEUE_

#include <mutex>
#include <optional>
#include <queue>

template <typename T> class ThreadSafeQueue {
  public:
    void push(T t) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(t);
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
};

#endif
