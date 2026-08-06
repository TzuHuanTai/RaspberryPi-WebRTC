#ifndef WHEP_SERVICE_H_
#define WHEP_SERVICE_H_

#include <memory>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include "args.h"
#include "rtc/conductor.h"
#include "signaling/peer_registry.h"
#include "signaling/signaling_service.h"

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

struct IceCandidates {
    std::string ice_ufrag;
    std::string ice_pwd;
    std::vector<std::string> candidates;
};

class WhepService : public SignalingService,
                    public std::enable_shared_from_this<WhepService> {
  public:
    static std::shared_ptr<WhepService> Create(Args args, std::shared_ptr<Conductor> conductor,
                                               boost::asio::io_context &ioc);

    WhepService(Args args, std::shared_ptr<Conductor> conductor, boost::asio::io_context &ioc);
    ~WhepService() override;

    void Connect() override;
    void Disconnect() override;

    webrtc::scoped_refptr<RtcPeer> CreatePeer(PeerConfig config = PeerConfig{});
    webrtc::scoped_refptr<RtcPeer> GetPeer(const std::string &peer_id);
    void RemovePeer(const std::string &peer_id);

  private:
    std::shared_ptr<Conductor> conductor_;
    uint16_t port_;
    tcp::acceptor acceptor_;
    PeerRegistry peer_registry_;

    void AcceptConnection();
};

class HttpSession : public std::enable_shared_from_this<HttpSession> {
  public:
    static std::shared_ptr<HttpSession> Create(tcp::socket socket,
                                               std::shared_ptr<WhepService> whep_service);

    HttpSession(tcp::socket socket, std::shared_ptr<WhepService> whep_service)
        : stream_(std::move(socket)),
          whep_service_(whep_service) {}
    ~HttpSession();

    void Start() { ReadRequest(); }

  private:
    std::shared_ptr<WhepService> whep_service_;

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<http::response<http::string_body>> res_;
    std::string content_type_;

    void ReadRequest();
    void WriteResponse();
    void CloseConnection();

    void HandleRequest();
    void HandlePostRequest();
    void HandlePatchRequest();
    void HandleOptionsRequest();
    void HandleDeleteRequest();
    void ResponseUnprocessableEntity(const char *message);
    void ResponseMethodNotAllowed();
    void ResponsePreconditionFailed();
    void SetCommonHeader(
        std::shared_ptr<boost::beast::http::response<boost::beast::http::string_body>> req);
    std::vector<std::string> ParseRoutes(std::string target);
    IceCandidates ParseCandidates(const std::string &sdp);
};

#endif
