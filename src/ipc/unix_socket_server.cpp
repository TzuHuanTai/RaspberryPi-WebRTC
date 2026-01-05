#include "ipc/unix_socket_server.h"

#include "common/logging.h"

#include <boost/cerrno.hpp>

std::shared_ptr<UnixSocketServer> UnixSocketServer::Create(const std::string &socket_path) {
    return std::make_shared<UnixSocketServer>(socket_path);
}

UnixSocketServer::UnixSocketServer(const std::string &socket_path)
    : server_fd_(-1),
      socket_path_(socket_path),
      running_(false) {}

UnixSocketServer::~UnixSocketServer() { Stop(); }

void UnixSocketServer::RegisterPeerCallback(const std::string &id, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    peer_callbacks_[id] = std::move(callback);
}

void UnixSocketServer::UnregisterPeerCallback(const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    peer_callbacks_.erase(id);
}

void UnixSocketServer::Write(const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &[fd, _] : client_threads_) {
        ::write(fd, message.c_str(), message.size());
    }
}

void UnixSocketServer::Start() {
    sockaddr_un addr;
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        perror("socket");
        return;
    }

    if (unlink(socket_path_.c_str()) == -1 && errno != ENOENT) {
        ERROR_PRINT("unlink %s failed %s", socket_path_.c_str(), strerror(errno));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, (sockaddr *)&addr, sizeof(addr)) == -1) {
        ERROR_PRINT("bind %s failed %s", socket_path_.c_str(), strerror(errno));
        close(server_fd_);
        return;
    }

    if (listen(server_fd_, 16) == -1) {
        perror("listen");
        close(server_fd_);
        return;
    }

    running_ = true;
    accept_thread_ = std::thread(&UnixSocketServer::AcceptLoop, this);
}

void UnixSocketServer::Stop() {
    running_ = false;

    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }

    if (accept_thread_.joinable())
        accept_thread_.join();

    std::unordered_map<int, std::thread> local_clients;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[fd, _] : client_threads_) {
            shutdown(fd, SHUT_RDWR);
        }

        local_clients.swap(client_threads_);
    }

    for (auto &[fd, thread] : local_clients) {
        if (thread.joinable())
            thread.join(); // already detached ones won't be in here
        close(fd);
    }

    unlink(socket_path_.c_str());
}

void UnixSocketServer::AcceptLoop() {
    while (running_) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (running_) {
                perror("accept");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }

        std::thread t(&UnixSocketServer::HandleClient, this, client_fd);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            client_threads_[client_fd] = std::move(t);
        }
    }
}

void UnixSocketServer::HandleClient(int client_fd) {
    char buffer[1024];
    while (running_) {
        int n = read(client_fd, buffer, sizeof(buffer));
        if (n <= 0) {
            break;
        }

        std::string msg(buffer, n);
        DEBUG_PRINT("[%d] Received: %s", client_fd, msg.c_str());

        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[_, callback] : peer_callbacks_) {
            if (callback) {
                callback(msg);
            }
        }
    }

    close(client_fd);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = client_threads_.find(client_fd);
        if (it != client_threads_.end()) {
            if (std::this_thread::get_id() == it->second.get_id()) {
                it->second.detach();
            }
            client_threads_.erase(it);
        }
    }

    DEBUG_PRINT("[%d] leaved!", client_fd);
}
