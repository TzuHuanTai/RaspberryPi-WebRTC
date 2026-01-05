#ifndef SUBJECT_H_
#define SUBJECT_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class Subscription {
  public:
    Subscription() = default;
    Subscription(std::function<void()> unsubscribe)
        : unsubscribe_(std::move(unsubscribe)) {}

    // Enable move semantics
    Subscription(Subscription &&) = default;
    Subscription &operator=(Subscription &&) = default;

    // Disable copy semantics
    Subscription(const Subscription &) = delete;
    Subscription &operator=(const Subscription &) = delete;

    ~Subscription() {
        if (unsubscribe_) {
            unsubscribe_();
        }
    }

  private:
    std::function<void()> unsubscribe_;
};

template <typename T> class Subject {
  public:
    using Callback = std::function<void(const T &)>;

    Subscription Subscribe(Callback callback) {
        auto observer = std::make_shared<Observer>();
        observer->callback = std::move(callback);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            observers_.push_back(observer);
        }

        return Subscription{[this, observer]() {
            std::lock_guard<std::mutex> lock(mutex_);
            observers_.erase(std::remove(observers_.begin(), observers_.end(), observer),
                             observers_.end());
        }};
    }

    void Next(const T &value) {
        std::vector<std::shared_ptr<Observer>> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = observers_;
        }

        for (auto &obs : snapshot) {
            obs->callback(value);
        }
    }

    size_t ObserverCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return observers_.size();
    }

  private:
    struct Observer {
        Callback callback;
    };

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Observer>> observers_;
};

#endif
