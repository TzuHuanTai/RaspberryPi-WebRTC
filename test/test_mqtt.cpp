#include "args.h"
#include "signaling/mqtt_service.h"

#include <unistd.h>

int main(int argc, char *argv[]) {
    Args args{.mqtt_username = "hakunamatata", .mqtt_password = "wonderful"};

    auto mqtt = MqttService::Create(args, nullptr, nullptr);

    sleep(60);
}
