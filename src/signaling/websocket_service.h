#ifndef WEBSOCKET_SERVICE_H_
#define WEBSOCKET_SERVICE_H_

#include "signaling/signaling_service.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <variant>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;
using WebSocketVariant = std::variant<websocket::stream<tcp::socket>, // non-TLS WebSocket
                                      websocket::stream<ssl::stream<tcp::socket>> // TLS WebSocket
                                      >;

class WebsocketService : public SignalingService {
  public:
    static std::shared_ptr<WebsocketService> Create(Args args, std::shared_ptr<Conductor> conductor,
                                                    boost::asio::io_context &ioc);

    WebsocketService(Args args, std::shared_ptr<Conductor> conductor, boost::asio::io_context &ioc);
    ~WebsocketService();

  protected:
    void Connect() override;
    void Disconnect() override;

  private:
    Args args_;
    WebSocketVariant ws_;
    tcp::resolver resolver_;
    beast::flat_buffer buffer_;
    std::deque<std::string> write_queue_;
    std::mutex write_mutex_;
    rtc::scoped_refptr<RtcPeer> pub_peer_;
    rtc::scoped_refptr<RtcPeer> sub_peer_;

    WebSocketVariant InitWebSocket(net::io_context &ioc);
    void OnResolve(beast::error_code ec, tcp::resolver::results_type results);
    void OnConnect(beast::error_code ec);
    void OnHandshake(beast::error_code ec);
    void OnHandshake(websocket::stream<tcp::socket> &ws);
    void OnHandshake(websocket::stream<ssl::stream<tcp::socket>> &ws);
    void OnMessage(const std::string &req);
    void OnRemoteIce(const std::string &message);
    void Read();
    void Write(const std::string &action, const std::string &message);
    void DoWrite();
};

#endif
