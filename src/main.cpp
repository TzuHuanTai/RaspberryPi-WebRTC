#include "args.h"
#include "common/logging.h"
#include "common/utils.h"
#include "parser.h"
#include "recorder/recorder_manager.h"
#include "rtc/conductor.h"
#include "signaling/http_service.h"
#include "signaling/mqtt_service.h"
#include "signaling/websocket_service.h"

int main(int argc, char *argv[]) {
    Args args;
    Parser::ParseArgs(argc, argv, args);

    std::shared_ptr<Conductor> conductor = Conductor::Create(args);
    std::unique_ptr<RecorderManager> bg_recorder_mgr;
    std::shared_ptr<RecorderManager> ondemand_recorder_mgr;

    // Background recorder
    if ((args.record_mode == RecordMode::Background || args.record_mode == -1) &&
        Utils::CreateFolder(args.record_path)) {
        bg_recorder_mgr =
            RecorderManager::Create(conductor->VideoSource(), conductor->AudioSource(), args);
        DEBUG_PRINT("Background recorder is running!");
    }

    // On-demand recorder
    if (args.record_mode == RecordMode::OnDemand || args.record_mode == -1) {
        Args ondemand_args = args;
        ondemand_args.record_path = args.record_ondemand_path;
        if (Utils::CreateFolder(ondemand_args.record_path)) {
            ondemand_recorder_mgr = RecorderManager::CreateOnDemand(
                conductor->VideoSource(), conductor->AudioSource(), ondemand_args);
            conductor->SetOnDemandRecorder(ondemand_recorder_mgr);
            DEBUG_PRINT("On-demand recorder is ready.");
        }
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
