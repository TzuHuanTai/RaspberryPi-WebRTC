#include "ipc/unix_socket_server.h"

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

    unlink(socket_path_.c_str());
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, (sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return;
    }

    if (listen(server_fd_, 16) == -1) {
        perror("listen");
        return;
    }

    running_ = true;
    accept_thread_ = std::thread(&UnixSocketServer::AcceptLoop, this);
}

void UnixSocketServer::Stop() {
    running_ = false;
    if (accept_thread_.joinable())
        accept_thread_.join();

    for (auto &[fd, t] : client_threads_) {
        close(fd);
        if (t.joinable())
            t.join();
    }

    if (server_fd_ >= 0)
        close(server_fd_);
    unlink(socket_path_.c_str());
}

void UnixSocketServer::AcceptLoop() {
    while (running_) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0)
            continue;
        std::lock_guard<std::mutex> lock(mutex_);
        client_threads_[client_fd] = std::thread(&UnixSocketServer::HandleClient, this, client_fd);
    }
}

void UnixSocketServer::HandleClient(int client_fd) {
    char buffer[1024];
    while (running_) {
        int n = read(client_fd, buffer, sizeof(buffer));
        if (n <= 0)
            break;

        std::string msg(buffer, n);
        std::cout << "[IPC] Received: " << msg << std::endl;

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &[_, callback] : peer_callbacks_) {
            if (callback) {
                callback(msg);
            }
        }
    }
    close(client_fd);
    std::lock_guard<std::mutex> lock(mutex_);
    client_threads_.erase(client_fd);
}
