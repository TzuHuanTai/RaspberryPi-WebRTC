#include "signaling/http_service.h"

#include <iostream>
#include <regex>
#include <vector>

#include "common/logging.h"

std::shared_ptr<HttpService> HttpService::Create(Args args, std::shared_ptr<Conductor> conductor) {
    return std::make_shared<HttpService>(args, conductor);
}

HttpService::HttpService(Args args, std::shared_ptr<Conductor> conductor)
    : SignalingService(conductor, true),
      port_(args.http_port),
      acceptor_({ioc_, {asio::ip::address_v6::any(), port_}}) {}

HttpService::~HttpService() {}

void HttpService::Connect() {
    INFO_PRINT("Http server is running on http://*:%d", port_);
    AcceptConnection();
    ioc_.run();
}

void HttpService::Disconnect() {}

void HttpService::AcceptConnection() {
    acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto session = HttpSession::Create(std::move(socket), shared_from_this());
            session->Start();
        } else {
            std::cerr << "Accept error: " << ec.message() << "\n";
        }
        AcceptConnection();
    });
}

std::shared_ptr<HttpSession> HttpSession::Create(tcp::socket socket,
                                                 std::shared_ptr<HttpService> http_service) {
    return std::make_shared<HttpSession>(std::move(socket), http_service);
}

HttpSession::~HttpSession() {}

void HttpSession::ReadRequest() {
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

void HttpSession::WriteResponse() {
    auto self = shared_from_this();
    http::async_write(stream_, *res_, [self](beast::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
            DEBUG_PRINT("Successfully response!");
            self->CloseConnection();
        } else {
            std::cerr << "Write error: " << ec.message() << "\n";
        }
    });
}

void HttpSession::CloseConnection() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    if (ec) {
        std::cerr << "Shutdown error: " << ec.message() << "\n";
    }
}

void HttpSession::HandleRequest() {
    DEBUG_PRINT("Receive http method: %d", req_.method());

    if (req_.method() != http::verb::options && req_.find("Content-Type") == req_.end()) {
        ResponseUnprocessableEntity("Without content type.");
        return;
    } else {
        content_type_ = req_["Content-Type"].to_string();
    }

    switch (req_.method()) {
        case http::verb::post:
            HandlePostRequest();
            break;
        case http::verb::patch:
            HandlePatchRequest();
            break;
        case http::verb::options:
            HandleOptionsRequest();
            break;
        case http::verb::delete_:
            HandleDeleteRequest();
            break;
        default:
            ResponseMethodNotAllowed();
            break;
    }
}

void HttpSession::HandlePostRequest() {
    if (content_type_ == "application/sdp") {
        auto peer = http_service_->CreatePeer();
        peer->OnLocalSdp([self = shared_from_this()](const std::string &peer_id,
                                                     const std::string &sdp,
                                                     const std::string &type) {
            std::string host = self->req_["Host"].to_string();
            std::string location = "https://" + host + "/resource/" + peer_id;
            self->res_ = std::make_shared<http::response<http::string_body>>(http::status::created,
                                                                             self->req_.version());
            self->SetCommonHeader(self->res_);
            self->res_->set(http::field::content_type, "application/sdp");
            self->res_->set(http::field::location, location);
            self->res_->body() = sdp;
            self->res_->prepare_payload();
            self->WriteResponse();
        });

        auto sdp = std::string(req_.body());
        peer->SetRemoteSdp(sdp, "offer");
    } else {
        ResponseUnprocessableEntity("The Content-Type only allow `application/sdp`.");
    }
}

void HttpSession::HandlePatchRequest() {
    auto routes = ParseRoutes(req_.target().to_string());

    if (content_type_ != "application/trickle-ice-sdpfrag" ||
        (routes.size() < 2 && routes[0] != "resource")) {
        ResponseUnprocessableEntity("The Content-Type only allow `trickle-ice-sdpfrag`.");
        return;
    }

    if (req_.find("If-Match") == req_.end()) {
        ResponsePreconditionFailed();
        return;
    }
    auto if_match = req_["If-Match"].to_string();

    auto peer_id = routes[1];
    auto peer = http_service_->GetPeer(peer_id);
    if (!peer) {
        ResponseUnprocessableEntity("The peer does not exist.");
        return;
    }

    auto sdp = std::string(req_.body());
    auto ice_group = ParseCandidates(sdp);
    for (const auto &candidate : ice_group.candidates) {
        DEBUG_PRINT("  Set remote ice: %s", candidate.c_str());
        peer->SetRemoteIce("0", 0, candidate);
    }

    DEBUG_PRINT("Set received candidates into peer (%s)!", peer_id.c_str());

    if (if_match == "*") {
        DEBUG_PRINT("peer (%s) ice restart!", peer_id.c_str());
        auto local_sdp = peer->RestartIce(ice_group.ice_ufrag, ice_group.ice_pwd);
        res_ =
            std::make_shared<http::response<http::string_body>>(http::status::ok, req_.version());

        res_->set(http::field::content_type, "application/trickle-ice-sdpfrag");
        res_->body() = local_sdp;

    } else {
        res_ = std::make_shared<http::response<http::string_body>>(http::status::no_content,
                                                                   req_.version());
    }

    SetCommonHeader(res_);
    res_->prepare_payload();
    WriteResponse();
}

void HttpSession::HandleOptionsRequest() {
    res_ = std::make_shared<http::response<http::string_body>>(http::status::no_content,
                                                               req_.version());
    SetCommonHeader(res_);
    res_->set(http::field::access_control_allow_headers,
              "Origin, X-Requested-With, Content-Type, Accept, Authorization");
    res_->set(http::field::access_control_allow_methods, "DELETE, OPTIONS, PATCH, POST");
    res_->set(http::field::access_control_allow_origin, "*");
    res_->prepare_payload();
    WriteResponse();
}

void HttpSession::HandleDeleteRequest() {
    auto routes = ParseRoutes(req_.target().to_string());

    if (routes.size() < 2 && routes[0] != "resource") {
        ResponseUnprocessableEntity("The resource is not appplicable.");
        return;
    }

    auto peer_id = routes[1];
    auto peer = http_service_->GetPeer(peer_id);
    if (!peer) {
        ResponseUnprocessableEntity("The peer does not exist.");
        return;
    }

    peer->Terminate();
    http_service_->RemovePeerFromMap(peer_id);
    DEBUG_PRINT("Close peer (%s)!", peer_id.c_str());

    res_ =
        std::make_shared<http::response<http::string_body>>(http::status::accepted, req_.version());
    SetCommonHeader(res_);
    res_->prepare_payload();
    WriteResponse();
}

void HttpSession::ResponseUnprocessableEntity(const char *message) {
    res_ = std::make_shared<http::response<http::string_body>>(http::status::unprocessable_entity,
                                                               req_.version());
    SetCommonHeader(res_);
    res_->set(http::field::content_type, "text/plain");
    res_->body() = message;
    res_->prepare_payload();
    WriteResponse();
}

void HttpSession::ResponseMethodNotAllowed() {
    res_ = std::make_shared<http::response<http::string_body>>(http::status::method_not_allowed,
                                                               req_.version());
    SetCommonHeader(res_);
    res_->set(http::field::content_type, "text/plain");
    res_->body() = "Only POST, DELETE, OPTIONS and PATCH method are allowed.";
    res_->prepare_payload();
    WriteResponse();
}

void HttpSession::ResponsePreconditionFailed() {
    res_ = std::make_shared<http::response<http::string_body>>(http::status::precondition_failed,
                                                               req_.version());
    SetCommonHeader(res_);
    res_->prepare_payload();
    WriteResponse();
}

void HttpSession::SetCommonHeader(
    std::shared_ptr<boost::beast::http::response<boost::beast::http::string_body>> res) {
    res->set(http::field::server, "piwebrtc.whep");
    res->set(http::field::access_control_allow_origin, "*");
}

std::vector<std::string> HttpSession::ParseRoutes(std::string target) {
    std::string tmp;
    std::vector<std::string> routes;
    std::stringstream ss(target);
    while (std::getline(ss, tmp, '/')) {
        if (!tmp.empty()) {
            routes.push_back(tmp);
        }
    }
    return routes;
}

IceCandidates HttpSession::ParseCandidates(const std::string &sdp) {
    std::regex midRegex(R"(a=mid:(\d+))");
    std::regex iceUfragRegex(R"(a=ice-ufrag:([^\s]+))");
    std::regex icePwdRegex(R"(a=ice-pwd:([^\s]+))");
    std::regex candidateRegex(R"(a=candidate:(.*))");

    std::smatch match;
    auto sdpBegin = sdp.begin();
    auto sdpEnd = sdp.end();

    IceCandidates result;

    while (std::regex_search(sdpBegin, sdpEnd, match, candidateRegex)) {
        std::string candidate = match[1].str();
        result.candidates.push_back("candidate:" + candidate);
        sdpBegin = match.suffix().first;
    }

    if (std::regex_search(sdp, match, iceUfragRegex)) {
        result.ice_ufrag = match[1].str();
        DEBUG_PRINT("ice-ufrag: %s", result.ice_ufrag.c_str());
    }

    if (std::regex_search(sdp, match, icePwdRegex)) {
        result.ice_pwd = match[1].str();
        DEBUG_PRINT("ice-pwd: %s", result.ice_pwd.c_str());
    }

    return result;
}
