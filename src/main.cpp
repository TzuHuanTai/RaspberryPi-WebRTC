#include <iostream>

#include "args.h"
#include "common/logging.h"
#include "common/utils.h"
#include "conductor.h"
#include "parser.h"
#include "recorder/recorder_manager.h"
#include "signaling/http_service.h"
#include "signaling/mqtt_service.h"
#include "signaling/websocket_service.h"

int main(int argc, char *argv[]) {
    Args args;
    Parser::ParseArgs(argc, argv, args);

    std::shared_ptr<Conductor> conductor = Conductor::Create(args);
    std::unique_ptr<RecorderManager> recorder_mgr;

    if (Utils::CreateFolder(args.record_path)) {
        recorder_mgr =
            RecorderManager::Create(conductor->VideoSource(), conductor->AudioSource(), args);
        DEBUG_PRINT("Recorder is running!");
    } else {
        DEBUG_PRINT("Recorder is not started!");
    }

    boost::asio::io_context ioc;
    auto work_guard = boost::asio::make_work_guard(ioc);

    std::vector<std::shared_ptr<SignalingService>> services;

    if (args.use_whep) {
        services.push_back(HttpService::Create(args, conductor, ioc));
    }

    if (args.use_websocket) {
        services.push_back(WebsocketService::Create(args, conductor, ioc));
    }

    if (args.use_mqtt) {
        services.push_back(MqttService::Create(args, conductor));
    }

    if (services.empty()) {
        ERROR_PRINT("No signaling service is running.");
        work_guard.reset();
    }

    for (auto &service : services) {
        service->Start();
    }

    ioc.run();

    return 0;
}
