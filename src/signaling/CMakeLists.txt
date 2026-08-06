project(signaling)

find_library(MOSQUITTO_LIBS NAMES mosquitto PATHS /usr/local/lib)
if(NOT MOSQUITTO_LIBS)
    message(FATAL_ERROR "Mosquitto library not found")
endif()

add_library(${PROJECT_NAME}
    peer_registry.cpp
    mqtt_service.cpp
    whep_service.cpp
    livekit_service.cpp
)

# Use boringssl in libwebrtc.a
target_link_libraries(${PROJECT_NAME} PUBLIC
    rtc
    ${MOSQUITTO_LIBS}
)
