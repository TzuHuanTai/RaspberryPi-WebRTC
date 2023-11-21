#include <iostream>
#include <string>
#include <unistd.h>
#include <chrono>
#include <ctime>

#include "args.h"
#include "conductor.h"
#include "parser.h"
#include "signaling/signalr_server.h"

int main(int argc, char *argv[]) {
    Args args;
    Parser::ParseArgs(argc, argv, args);

    auto conductor = Conductor::Create(args);

    while (true) {
        conductor->CreatePeerConnection();

        auto signal = SignalrService::Create(args.signaling_url, conductor);

        std::cout << "=> main: wait for ready streaming!" << std::endl;
        std::unique_lock<std::mutex> lock(conductor->mtx);
        conductor->cond_var.wait(lock, [&]{return conductor->is_ready_for_streaming;});

        auto start = std::chrono::system_clock::now();
        std::time_t end_time;
        while (conductor->is_ready_for_streaming) {
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = end-start;
            end_time = std::chrono::system_clock::to_time_t(end);
            std::cout << '\r' << "running time: " << elapsed_seconds.count() << "s " << "\e[K" << std::flush;
            usleep(100000);
        }
        
        std::cout << std::ctime(&end_time);
    }
    
    return 0;
}
