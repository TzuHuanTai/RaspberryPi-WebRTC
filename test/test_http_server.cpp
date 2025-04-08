#include "signaling/http_service.h"

int main() {
    Args args;

    boost::asio::io_context ioc;

    boost::asio::steady_timer timer(ioc, std::chrono::seconds(5));
    timer.async_wait([&ioc](const boost::system::error_code &ec) {
        if (!ec) {
            std::cout << "no events in io_context" << std::endl;
            ioc.stop();
        }
    });

    auto http_service = HttpService::Create(args, nullptr, ioc);
    http_service->Start();

    ioc.run();
    return 0;
}
