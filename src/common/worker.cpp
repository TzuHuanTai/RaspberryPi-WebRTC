#include "common/worker.h"
#include "common/logging.h"

Worker::Worker(std::string name, std::function<void()> executing_function)
    : abort_(false),
      name_(name),
      executing_function_(executing_function) {}

Worker::~Worker() {
    abort_.store(true);
    thread_.Finalize();
    DEBUG_PRINT("'%s' was released!", name_.c_str());
}

void Worker::Run() {
    thread_ = rtc::PlatformThread::SpawnJoinable(
        [this]() {
            this->Thread();
        },
        name_, rtc::ThreadAttributes().SetPriority(rtc::ThreadPriority::kHigh));
}

void Worker::Thread() {
    while (!abort_.load()) {
        executing_function_();
    }
}
