#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <string>

#include <boost/asio/buffers_iterator.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

void OnHandshake(boost::system::error_code ec,
                 websocket::stream<beast::ssl_stream<tcp::socket>> &ws) {
    if (!ec) {
        printf("WebSocket is connected!\n");

        while (true) {
            beast::flat_buffer buffer;
            try {
                ws.read(buffer);
            } catch (const std::exception &e) {
                std::cerr << "Error: " << e.what() << std::endl;
                break;
            }

            if (ws.got_text()) {
                std::string res = beast::buffers_to_string(buffer.data());

                json jsonObj = json::parse(res.c_str());
                std::string action = jsonObj["action"];
                std::string message = jsonObj["message"];
                std::cout << "Received Action: " << action << std::endl;
                std::cout << "Received Message: " << message << std::endl;

                if (action == "join") {
                    std::string response = "{\"Action\":\"offer\",\"Message\":\"test\"}";
                    ws.write(net::buffer(response));
                } else {
                    break;
                }
            } else if (ws.got_binary()) {
                std::vector<uint8_t> binaryData(boost::asio::buffers_begin(buffer.data()),
                                                boost::asio::buffers_end(buffer.data()));
                std::cout << "Received Binary Data (size: " << binaryData.size() << " bytes)"
                          << std::endl;
            }
        }

        // ws.close(websocket::close_code::normal);
    } else {
        printf("Handshake Error: %s", ec.message().c_str());
    }
}

int main() {
    net::io_context ioc;

    net::ssl::context ctx(net::ssl::context::tls_client);
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(net::ssl::verify_peer);

    tcp::resolver resolver(ioc);
    websocket::stream<beast::ssl_stream<tcp::socket>> ws(ioc, ctx);

    std::string host = "api.picamera.live";
    std::string port = "443";
    std::string accessToken =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJleHAiOjE3NzM4OTI2NjEsImlzcyI6IkFQSVduUVRzNHRtVVp2QSIsIm5iZiI6MTc0MjM1NjY2MSwic3ViIj"
        "oiMTMxZGZjMzItNjRlMi00YjZiLTllZGEtZDdjYTU5NTNjYWJlIiwidmlkZW8iOnsicm9vbSI6ImRldmljZS0x"
        "Iiwicm9vbUpvaW4iOnRydWV9fQ.ThQTHYd8CBR0t3epwcak6oaleeu760V96UF8GbOMUks";
    std::string target = "/rtc?token=" + accessToken;

    auto const results = resolver.resolve(host, port);

    net::async_connect(
        beast::get_lowest_layer(ws), results, [&](boost::system::error_code ec, tcp::endpoint) {
            if (ec) {
                std::cerr << "Connect Error: " << ec.message() << std::endl;
            } else {
                ws.next_layer().async_handshake(
                    net::ssl::stream_base::client, [&](boost::system::error_code ec) {
                        if (ec) {
                            return;
                        }

                        ws.async_handshake(host, target, [&](boost::system::error_code ec) {
                            OnHandshake(ec, ws);
                        });
                    });
            }
        });

    ioc.run();
    return 0;
}
