#include "signaling/websocket_service.h"

int main() {
    Args args{
        .ws_token =
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
            "eyJleHAiOjE3NzM4OTI2NjEsImlzcyI6IkFQSVduUVRzNHRtVVp2QSIsIm5iZiI6MTc0MjM1NjY2MSwic3ViIj"
            "oiMTMxZGZjMzItNjRlMi00YjZiLTllZGEtZDdjYTU5NTNjYWJlIiwidmlkZW8iOnsicm9vbSI6ImRldmljZS0x"
            "Iiwicm9vbUpvaW4iOnRydWV9fQ.ThQTHYd8CBR0t3epwcak6oaleeu760V96UF8GbOMUks"};

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
