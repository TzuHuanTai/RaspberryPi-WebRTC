#include "signaling/websocket_service.h"

int main() {
    Args args{
        .uid = "test_device",
        .ws_key = "",
        .ws_host = "localhost",
        .ws_room = "",
    };

    net::io_context ioc;

    boost::asio::steady_timer timer(ioc, std::chrono::seconds(5));
    timer.async_wait([&ioc](const boost::system::error_code &ec) {
        if (!ec) {
            std::cout << "no events in io_context" << std::endl;
            ioc.stop();
        }
    });

    auto ws_service = WebsocketService::Create(args, nullptr, ioc);
    ws_service->Start();

    ioc.run();
    return 0;
}
