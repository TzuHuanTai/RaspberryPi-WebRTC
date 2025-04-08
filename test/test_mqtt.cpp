#include "args.h"
#include "signaling/mqtt_service.h"

#include <unistd.h>

int main(int argc, char *argv[]) {
    Args args{.mqtt_username = "hakunamatata", .mqtt_password = "wonderful"};

    auto mqtt_service = MqttService::Create(args, nullptr);
    mqtt_service->Start();

    return 0;
}
