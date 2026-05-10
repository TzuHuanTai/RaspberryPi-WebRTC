#ifndef LOGGING_H
#define LOGGING_H

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>
#include <string>
#include <unistd.h>

inline std::string GetFolderName(const std::string &file_path) {
    return std::filesystem::path(file_path).parent_path().filename().string();
}

inline std::string GetFileName(const std::string &file_path) {
    return std::filesystem::path(file_path).filename().string();
}

inline std::string GetCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto zoned_time = std::chrono::zoned_time{std::chrono::current_zone(), now};
    return std::format("{:%T}", zoned_time);
}

#ifdef DEBUG_MODE
#define DEBUG_PRINT(fmt, ...)                                                                      \
    printf("[%s] [%d] \033[1;33mDEBUG\033[0m \033[1;37m%s\033[0m \033[1;34m%s:%d\033[0m " fmt      \
           "\n",                                                                                   \
           GetCurrentTime().c_str(), (int)gettid(), GetFolderName(__FILE__).c_str(),               \
           GetFileName(__FILE__).c_str(), __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#define ERROR_PRINT(fmt, ...)                                                                      \
    fprintf(stderr,                                                                                \
            "[%s] [%d] \033[1;31mERROR\033[0m \033[1;37m%s\033[0m \033[1;34m%s:%d\033[0m " fmt     \
            "\n",                                                                                  \
            GetCurrentTime().c_str(), (int)gettid(), GetFolderName(__FILE__).c_str(),              \
            GetFileName(__FILE__).c_str(), __LINE__, ##__VA_ARGS__)

#define INFO_PRINT(fmt, ...)                                                                       \
    printf("[%s] [%d] \033[1;32m INFO\033[0m \033[1;37m%s\033[0m \033[1;34m%s:%d\033[0m " fmt      \
           "\n",                                                                                   \
           GetCurrentTime().c_str(), (int)gettid(), GetFolderName(__FILE__).c_str(),               \
           GetFileName(__FILE__).c_str(), __LINE__, ##__VA_ARGS__)

#endif // LOGGING_H
