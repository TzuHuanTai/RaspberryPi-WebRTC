#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <memory>
#include <string>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

class HttpSession : public std::enable_shared_from_this<HttpSession> {
  public:
    explicit HttpSession(tcp::socket socket)
        : stream_(std::move(socket)) {}

    void Start() { ReadRequest(); }

  private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<http::response<http::string_body>> res_;

    void ReadRequest() {
        auto self = shared_from_this();
        http::async_read(stream_, buffer_, req_,
                         [self](beast::error_code ec, std::size_t bytes_transferred) {
                             if (!ec) {
                                 self->HandleRequest();
                             } else {
                                 std::cerr << "Read error: " << ec.message() << "\n";
                             }
                         });
    }

    void HandleRequest() {
        // Prepare the response
        res_ =
            std::make_shared<http::response<http::string_body>>(http::status::ok, req_.version());
        res_->set(http::field::server, "Boost.Beast/HTTP");
        res_->set(http::field::content_type, "text/plain");
        res_->body() = "Hello, world!";
        res_->prepare_payload();

        WriteResponse();
    }

    void WriteResponse() {
        auto self = shared_from_this();
        http::async_write(stream_, *res_,
                          [self](beast::error_code ec, std::size_t bytes_transferred) {
                              if (!ec) {
                                  std::cout << "Response sent successfully\n";
                                  self->CloseConnection();
                              } else {
                                  std::cerr << "Write error: " << ec.message() << "\n";
                              }
                          });
    }

    void CloseConnection() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        if (ec) {
            std::cerr << "Shutdown error: " << ec.message() << "\n";
        }
    }
};

class HttpServer {
  public:
    HttpServer(asio::io_context &ioc, const tcp::endpoint &endpoint)
        : acceptor_(ioc, endpoint) {}

    void Start() { AcceptConnection(); }

  private:
    tcp::acceptor acceptor_;

    void AcceptConnection() {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<HttpSession>(std::move(socket))->Start();
            } else {
                std::cerr << "Accept error: " << ec.message() << "\n";
            }
            AcceptConnection();
        });
    }
};

int main() {
    try {
        asio::io_context ioc;

        // Server listens on port 8080
        tcp::endpoint endpoint(tcp::v4(), 8080);
        HttpServer server(ioc, endpoint);

        std::cout << "Server is running on port 8080...\n";

        server.Start();
        ioc.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}
