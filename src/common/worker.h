#ifndef WORKER_H_
#define WORKER_H_

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
    rtc::PlatformThread thread_;

    void Thread();
};

#endif // WORKER_H_
