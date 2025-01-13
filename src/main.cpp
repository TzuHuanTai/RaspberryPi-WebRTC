#include <iostream>

#include "args.h"
#include "common/logging.h"
#include "common/utils.h"
#include "conductor.h"
#include "parser.h"
#include "recorder/recorder_manager.h"
#include "signaling/signaling_service.h"
#if USE_MQTT_SIGNALING
#include "signaling/mqtt_service.h"
#elif USE_HTTP_SIGNALING
#include "signaling/http_service.h"
#endif

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

    auto signaling_service = ([args, conductor]() -> std::shared_ptr<SignalingService> {
#if USE_MQTT_SIGNALING
        return MqttService::Create(args, conductor);
#elif USE_HTTP_SIGNALING
        return HttpService::Create(args, conductor);
#else
        return nullptr;
#endif
    })();

    if (signaling_service) {
        signaling_service->Start();
    } else {
        INFO_PRINT("There is no any signaling service found!");
    }

    return 0;
}
