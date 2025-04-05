project(signaling)

find_library(MOSQUITTO_LIBS NAMES mosquitto PATHS /usr/local/lib)
if(NOT MOSQUITTO_LIBS)
    message(FATAL_ERROR "Mosquitto library not found")
endif()

add_library(${PROJECT_NAME}
    mqtt_service.cpp
    http_service.cpp
    websocket_service.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${MOSQUITTO_LIBS})
