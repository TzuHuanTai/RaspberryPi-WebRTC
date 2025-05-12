#include "ipc/unix_socket_server.h"

#include "args.h"

int main() {
    Args args;

    auto server = UnixSocketServer::Create(args.socket_path);
    server->Start();
    std::this_thread::sleep_for(std::chrono::minutes(10));
    server->Stop();
    return 0;
}
