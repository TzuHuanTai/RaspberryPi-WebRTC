#include "signaling/http_client.h"

#include <algorithm>
#include <memory>
#include <variant>

#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

namespace {

struct HttpUrl {
    bool use_tls = false;
    std::string host;
    uint16_t port = 0;
    std::string target = "/";
};

// Splits an absolute http/https url. The port falls back to the scheme default, and IPv6
// literals keep their brackets out of `host` so it can be handed to a resolver as-is.
bool ParseHttpUrl(const std::string &url, HttpUrl &out) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return false;
    }

    std::string scheme = url.substr(0, scheme_end);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
    if (scheme == "http") {
        out.use_tls = false;
        out.port = 80;
    } else if (scheme == "https") {
        out.use_tls = true;
        out.port = 443;
    } else {
        return false;
    }

    std::string rest = url.substr(scheme_end + 3);
    auto path_start = rest.find('/');
    std::string authority = rest.substr(0, path_start);
    out.target = path_start == std::string::npos ? "/" : rest.substr(path_start);

    if (!authority.empty() && authority.front() == '[') { // IPv6 literal
        auto bracket_end = authority.find(']');
        if (bracket_end == std::string::npos) {
            return false;
        }
        out.host = authority.substr(1, bracket_end - 1);
        if (bracket_end + 1 < authority.size() && authority[bracket_end + 1] == ':') {
            out.port = static_cast<uint16_t>(std::stoi(authority.substr(bracket_end + 2)));
        }
    } else {
        auto colon = authority.find(':');
        out.host = authority.substr(0, colon);
        if (colon != std::string::npos) {
            out.port = static_cast<uint16_t>(std::stoi(authority.substr(colon + 1)));
        }
    }

    return !out.host.empty();
}

// The SSL context created via boost::asio::ssl::context uses the underlying BoringSSL
// implementation shipped inside libwebrtc.a. BoringSSL is not a drop-in replacement for
// OpenSSL and does not implement all OpenSSL APIs, so keep this to the basics.
class Session : public std::enable_shared_from_this<Session> {
  public:
    Session(net::io_context &ioc, HttpUrl url, HttpClient::Request request,
            HttpClient::OnCompleteFunc on_complete)
        : url_(std::move(url)),
          request_(std::move(request)),
          on_complete_(std::move(on_complete)),
          ssl_ctx_(ssl::context::tls_client),
          resolver_(net::make_strand(ioc)),
          stream_(
              url_.use_tls
                  ? SocketVariant(std::in_place_type<beast::ssl_stream<beast::tcp_stream>>,
                                  net::make_strand(ioc), ssl_ctx_)
                  : SocketVariant(std::in_place_type<beast::tcp_stream>, net::make_strand(ioc))) {
        if (url_.use_tls) {
            ssl_ctx_.set_default_verify_paths();
            ssl_ctx_.set_verify_mode(ssl::verify_peer);
        }
    }

    void Start();

  private:
    using SocketVariant = std::variant<beast::tcp_stream, beast::ssl_stream<beast::tcp_stream>>;

    HttpUrl url_;
    HttpClient::Request request_;
    HttpClient::OnCompleteFunc on_complete_;
    ssl::context ssl_ctx_;
    tcp::resolver resolver_;
    SocketVariant stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    bool completed_ = false;

    void OnResolve(beast::error_code ec, tcp::resolver::results_type results);
    void OnConnect(beast::error_code ec);
    void OnTlsHandshake(beast::error_code ec);
    void Write();
    void Read();
    void OnRead(beast::error_code ec);
    void Finish(bool ok, int status, const std::string &body);
    void Shutdown();
};

void Session::Start() {
    std::string host_header = url_.host;
    if (!(url_.port == 80 && !url_.use_tls) && !(url_.port == 443 && url_.use_tls)) {
        host_header += ":" + std::to_string(url_.port);
    }

    req_.version(11);
    req_.method(request_.method);
    req_.target(url_.target);
    req_.set(http::field::host, host_header);
    req_.set(http::field::user_agent, "pi-webrtc");
    req_.set(http::field::content_type, request_.content_type);
    req_.set(http::field::accept, "*/*");
    if (!request_.bearer_token.empty()) {
        req_.set(http::field::authorization, "Bearer " + request_.bearer_token);
    }
    req_.body() = request_.body;
    req_.prepare_payload();

    auto self = shared_from_this();
    resolver_.async_resolve(url_.host, std::to_string(url_.port),
                            [self](beast::error_code ec, tcp::resolver::results_type results) {
                                self->OnResolve(ec, results);
                            });
}

void Session::OnResolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        Finish(false, 0, "Failed to resolve " + url_.host + ": " + ec.message());
        return;
    }

    auto self = shared_from_this();
    std::visit(
        [this, self, &results](auto &stream) {
            auto &lowest = beast::get_lowest_layer(stream);
            lowest.expires_after(std::chrono::seconds(request_.timeout_sec));
            lowest.async_connect(results, [self](beast::error_code ec, tcp::endpoint) {
                self->OnConnect(ec);
            });
        },
        stream_);
}

void Session::OnConnect(beast::error_code ec) {
    if (ec) {
        Finish(false, 0, "Failed to connect to " + url_.host + ": " + ec.message());
        return;
    }

    if (!url_.use_tls) {
        Write();
        return;
    }

    auto &stream = std::get<beast::ssl_stream<beast::tcp_stream>>(stream_);
    if (!SSL_set_tlsext_host_name(stream.native_handle(), url_.host.c_str())) {
        Finish(false, 0, "Failed to set SNI hostname");
        return;
    }

    auto self = shared_from_this();
    stream.async_handshake(ssl::stream_base::client, [self](beast::error_code ec) {
        self->OnTlsHandshake(ec);
    });
}

void Session::OnTlsHandshake(beast::error_code ec) {
    if (ec) {
        Finish(false, 0, "Failed to tls handshake with " + url_.host + ": " + ec.message());
        return;
    }
    Write();
}

void Session::Write() {
    auto self = shared_from_this();
    std::visit(
        [this, self](auto &stream) {
            beast::get_lowest_layer(stream).expires_after(
                std::chrono::seconds(request_.timeout_sec));
            http::async_write(stream, req_, [self](beast::error_code ec, std::size_t) {
                if (ec) {
                    self->Finish(false, 0, "Failed to send request: " + ec.message());
                    return;
                }
                self->Read();
            });
        },
        stream_);
}

void Session::Read() {
    auto self = shared_from_this();
    std::visit(
        [this, self](auto &stream) {
            http::async_read(stream, buffer_, res_, [self](beast::error_code ec, std::size_t) {
                self->OnRead(ec);
            });
        },
        stream_);
}

void Session::OnRead(beast::error_code ec) {
    if (ec) {
        Finish(false, 0, "Failed to read response: " + ec.message());
        return;
    }

    auto status = res_.result_int();
    Finish(status >= 200 && status < 300, status, res_.body());
}

void Session::Finish(bool ok, int status, const std::string &body) {
    if (completed_) {
        return;
    }
    completed_ = true;

    Shutdown();

    if (on_complete_) {
        auto callback = std::move(on_complete_);
        callback(HttpClient::Response{ok, status, body});
    }
}

void Session::Shutdown() {
    boost::system::error_code ec;
    std::visit(
        [&ec](auto &stream) {
            auto &lowest = beast::get_lowest_layer(stream);
            lowest.expires_never();
            lowest.socket().shutdown(tcp::socket::shutdown_both, ec);
            lowest.socket().close(ec);
        },
        stream_);
}

} // namespace

void HttpClient::Send(net::io_context &ioc, Request request, OnCompleteFunc on_complete) {
    HttpUrl parsed;
    if (!ParseHttpUrl(request.url, parsed)) {
        on_complete(Response{false, 0, "Invalid url: " + request.url});
        return;
    }

    auto session = std::make_shared<Session>(ioc, std::move(parsed), std::move(request),
                                             std::move(on_complete));
    session->Start();
}
