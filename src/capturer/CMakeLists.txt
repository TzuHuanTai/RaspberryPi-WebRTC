project(capturer)

find_package(PkgConfig)
pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera)
message(STATUS "libcamera library found:")
message(STATUS "    version: ${LIBCAMERA_VERSION}")
message(STATUS "    libraries: ${LIBCAMERA_LINK_LIBRARIES}")
message(STATUS "    include path: ${LIBCAMERA_INCLUDE_DIRS}")

pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
pkg_check_modules(PULSE_SIMPLE REQUIRED IMPORTED_TARGET libpulse-simple)
message(STATUS "PulseAudio library found:")
message(STATUS "    PULSE libs: ${PULSE_LINK_LIBRARIES}")
message(STATUS "    PULSE_SIMPLE libs: ${PULSE_SIMPLE_LINK_LIBRARIES}")

aux_source_directory(${PROJECT_SOURCE_DIR} CAPTURE_FILES)

add_library(${PROJECT_NAME} ${CAPTURE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${LIBCAMERA_INCLUDE_DIRS}
    ${PULSE_INCLUDE_DIRS}
    ${PULSE_SIMPLE_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    common
    v4l2_codecs
    ${WEBRTC_LIBRARY}
    ${LIBCAMERA_LINK_LIBRARIES}
    ${PULSE_LINK_LIBRARIES}
    ${PULSE_SIMPLE_LINK_LIBRARIES}
)
