#include "signaling/websocket_service.h"

#include <nlohmann/json.hpp>

std::shared_ptr<WebsocketService>
WebsocketService::Create(Args args, std::shared_ptr<Conductor> conductor, net::io_context &ioc) {
    return std::make_shared<WebsocketService>(args, conductor, ioc);
}

std::string WebsocketService::UrlEncode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (const char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << std::uppercase << int(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

std::string
WebsocketService::BuildWebSocketTarget(const std::string &basePath,
                                       const std::map<std::string, std::string> &params) {
    std::ostringstream target;
    target << basePath;

    if (!params.empty()) {
        target << "?";
        bool first = true;
        for (const auto &[key, value] : params) {
            if (!first)
                target << "&";
            target << key << "=" << UrlEncode(value);
            first = false;
        }
    }
    return target.str();
}

WebsocketService::WebsocketService(Args args, std::shared_ptr<Conductor> conductor,
                                   net::io_context &ioc)
    : SignalingService(conductor),
      args_(args),
      ws_(InitWebSocket(ioc)),
      resolver_(net::make_strand(ioc)),
      ping_timer_(ioc) {}

WebsocketService::~WebsocketService() { Disconnect(); }

WebSocketVariant WebsocketService::InitWebSocket(net::io_context &ioc) {
    if (args_.use_tls) {
        // The SSL context created via boost::asio::ssl::context uses the underlying BoringSSL
        // implementation (when linked with WebRTC or other BoringSSL-based libraries). BoringSSL is
        // not a drop-in replacement for OpenSSL and does not implement all OpenSSL APIs. As a
        // result, certain methods may be unsupported or behave differently.
        // Ensure that only compatible OpenSSL APIs are used when BoringSSL is present.
        DEBUG_PRINT("Using TLS WebSocket, SSL version: %s", OpenSSL_version(OPENSSL_VERSION));

        ssl::context ctx(ssl::context::tls);
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        return websocket::stream<ssl::stream<tcp::socket>>(net::make_strand(ioc), ctx);
    } else {
        return websocket::stream<tcp::socket>(net::make_strand(ioc));
    }
}

void WebsocketService::Connect() {
    auto port = args_.use_tls ? 443 : 80;
    INFO_PRINT("Connect to WebSocket %s:%d", args_.ws_host.c_str(), port);

    resolver_.async_resolve(
        args_.ws_host, std::to_string(port),
        [this](boost::system::error_code ec, tcp::resolver::results_type results) {
            OnResolve(ec, results);
        });
}

void WebsocketService::Disconnect() {
    ping_timer_.cancel();

    std::visit(
        [](auto &ws) {
            if (ws.is_open()) {
                ws.async_close(websocket::close_code::normal, [](boost::system::error_code ec) {
                    if (ec) {
                        ERROR_PRINT("Close Error: %s", ec.message().c_str());
                    } else {
                        INFO_PRINT("WebSocket Closed");
                    }
                });
            } else {
                INFO_PRINT("WebSocket already closed");
            }
        },
        ws_);
}

void WebsocketService::OnResolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        ERROR_PRINT("Failed to resolve: %s", ec.message().c_str());
        return;
    }

    std::visit(
        [this, results](auto &ws) {
            net::async_connect(beast::get_lowest_layer(ws), results,
                               [this, &ws](boost::system::error_code ec, tcp::endpoint) {
                                   OnConnect(ec);
                               });
        },
        ws_);
}

void WebsocketService::OnConnect(beast::error_code ec) {
    if (ec) {
        ERROR_PRINT("Failed to connect: %s", ec.message().c_str());
        return;
    }

    std::visit(
        [this](auto &ws) {
            OnHandshake(ws);
        },
        ws_);
}

void WebsocketService::OnHandshake(websocket::stream<tcp::socket> &ws) {
    std::string target = BuildWebSocketTarget("/rtc", {{"apiKey", args_.ws_key},
                                                       {"roomId", args_.ws_room},
                                                       {"userId", args_.uid},
                                                       {"canSubscribe", "0"}});
    ws.async_handshake(args_.ws_host, target, [this](boost::system::error_code ec) {
        OnHandshake(ec);
    });
}

void WebsocketService::OnHandshake(websocket::stream<ssl::stream<tcp::socket>> &ws) {
    ws.next_layer().async_handshake(
        ssl::stream_base::client, [this, &ws](boost::system::error_code ec) {
            if (ec) {
                ERROR_PRINT("Failed to tls handshake: %s", ec.message().c_str());
            }
            std::string target = BuildWebSocketTarget("/rtc", {{"apiKey", args_.ws_key},
                                                               {"roomId", args_.ws_room},
                                                               {"userId", args_.uid},
                                                               {"canSubscribe", "0"}});
            ws.async_handshake(args_.ws_host, target, [this](boost::system::error_code ec) {
                OnHandshake(ec);
            });
        });
}

void WebsocketService::OnHandshake(beast::error_code ec) {
    if (ec) {
        ERROR_PRINT("Failed to handshake: %s", ec.message().c_str());
        return;
    }

    Read();
    ScheduleNextPing();
}

void WebsocketService::Read() {
    std::visit(
        [this](auto &ws) {
            if (!ws.is_open()) {
                return;
            }

            ws.async_read(buffer_,
                          [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                              if (ec) {
                                  ERROR_PRINT("Failed to read: %s", ec.message().c_str());
                                  Disconnect();
                                  return;
                              }
                              std::string req = beast::buffers_to_string(buffer_.data());
                              OnMessage(req);
                              buffer_.consume(bytes_transferred);
                              Read();
                          });
        },
        ws_);
}

void WebsocketService::OnMessage(const std::string &req) {
    DEBUG_PRINT("Received message: %s", req.c_str());
    try {
        nlohmann::json jsonObj = nlohmann::json::parse(req.c_str());
    } catch (const std::exception &e) {
        ERROR_PRINT("Failed to parse message: %s", e.what());
        return;
    }

    json jsonObj = json::parse(req.c_str());
    std::string action = jsonObj["action"];
    std::string message = jsonObj["message"];

    if (action == "join") {
        PeerConfig config;
        config.is_sfu_peer = true;

        webrtc::PeerConnectionInterface::IceServer ice_server;
        nlohmann::json messageJson = nlohmann::json::parse(jsonObj["message"].get<std::string>());
        ice_server.urls = messageJson["urls"].get<std::vector<std::string>>();
        ice_server.username = messageJson["username"];
        ice_server.password = messageJson["credential"];
        config.servers.push_back(ice_server);

        pub_peer_ = CreatePeer(config);
        if (pub_peer_) {
            pub_peer_->OnLocalSdp([this](const std::string &peer_id, const std::string &sdp,
                                         const std::string &type) {
                Write(type, sdp);
            });
            pub_peer_->OnLocalIce([this](const std::string &peer_id, const std::string &sdp_mid,
                                         int sdp_mline_index, const std::string &candidate) {
                Write("tricklePublisher", candidate);
            });
        }

        config.is_publisher = false;
        sub_peer_ = CreatePeer(config);
        if (sub_peer_) {
            sub_peer_->OnLocalSdp([this](const std::string &peer_id, const std::string &sdp,
                                         const std::string &type) {
                Write(type, sdp);
            });
            sub_peer_->OnLocalIce([this](const std::string &peer_id, const std::string &sdp_mid,
                                         int sdp_mline_index, const std::string &candidate) {
                Write("trickleSubscriber", candidate);
            });
        }

        Write("addVideoTrack", args_.uid);
        if (!args_.no_audio) {
            Write("addAudioTrack", args_.uid);
        }

        pub_peer_->CreateOffer();

    } else if (action == "offer" && sub_peer_ && !sub_peer_->IsConnected()) {
        sub_peer_->SetRemoteSdp(message, "offer");
    } else if (action == "answer" && pub_peer_ && !pub_peer_->IsConnected()) {
        pub_peer_->SetRemoteSdp(message, "answer");
    } else if (action == "trickle") {
        OnRemoteIce(message);
    } else if (action == "leave") {
        Disconnect();
    }
}

void WebsocketService::OnRemoteIce(const std::string &message) {
    nlohmann::json res = nlohmann::json::parse(message);
    std::string target = res["target"];
    std::string canditateInit = res["candidateInit"];

    nlohmann::json canditateObj = nlohmann::json::parse(canditateInit);
    std::string sdp_mid = canditateObj["sdpMid"];
    int sdp_mline_index = canditateObj["sdpMLineIndex"];
    std::string candidate = canditateObj["candidate"];
    DEBUG_PRINT("Received remote ICE: %s, %d, %s", sdp_mid.c_str(), sdp_mline_index,
                candidate.c_str());

    if (target == "PUBLISHER") {
        pub_peer_->SetRemoteIce(sdp_mid, sdp_mline_index, candidate);
    } else if (target == "SUBSCRIBER") {
        sub_peer_->SetRemoteIce(sdp_mid, sdp_mline_index, candidate);
    }
}

void WebsocketService::ScheduleNextPing() {
    ping_timer_.expires_after(std::chrono::seconds(15));
    ping_timer_.async_wait([this](const boost::system::error_code &ec) {
        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                ERROR_PRINT("Ping timer error: %s", ec.message().c_str());
            }
            return;
        }

        Write("ping", "");
        ScheduleNextPing();
    });
}

void WebsocketService::Write(const std::string &action, const std::string &message) {
    nlohmann::json request_json;
    request_json["action"] = action;
    request_json["message"] = message;
    std::string request = request_json.dump();

    std::lock_guard<std::mutex> lock(write_mutex_);
    bool writing_in_progress = !write_queue_.empty();
    write_queue_.push_back(request);

    if (!writing_in_progress) {
        DoWrite();
    }
}

void WebsocketService::DoWrite() {
    if (write_queue_.empty())
        return;

    std::visit(
        [this](auto &ws) {
            ws.async_write(net::buffer(write_queue_.front()),
                           [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                               std::lock_guard<std::mutex> lock(write_mutex_);
                               if (ec) {
                                   ERROR_PRINT("Failed to write: %s", ec.message().c_str());
                                   Disconnect();
                               }

                               write_queue_.pop_front();

                               if (!write_queue_.empty()) {
                                   DoWrite();
                               }
                           });
        },
        ws_);
}
