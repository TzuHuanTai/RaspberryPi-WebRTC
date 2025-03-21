project(signaling)

find_library(MOSQUITTO_LIBS NAMES mosquitto PATHS /usr/local/lib)
if(NOT MOSQUITTO_LIBS)
    message(FATAL_ERROR "Mosquitto library not found")
endif()

add_library(${PROJECT_NAME}
    mqtt_service.cpp
    http_service.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${MOSQUITTO_LIBS})
