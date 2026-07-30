#ifndef COMMON_WORKER_H_
#define COMMON_WORKER_H_

#include <atomic>
#include <functional>
#include <string>

#include <rtc_base/platform_thread.h>

class Worker {
  public:
    Worker(std::string name, std::function<void()> executing_function);
    ~Worker();
    void Run();

  private:
    std::atomic<bool> abort_;
    std::string name_;
    std::function<void()> executing_function_;
    webrtc::PlatformThread thread_;

    void Thread();
};

#endif // COMMON_WORKER_H_
