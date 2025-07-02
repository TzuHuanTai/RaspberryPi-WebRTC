project(capturer)

find_package(PkgConfig REQUIRED)

# Source files
set(CAPTURE_FILES
    v4l2_capturer.cpp
    pa_capturer.cpp
)

if(${PLATFORM} STREQUAL "jetson")
    list(APPEND CAPTURE_FILES libargus_capturer.cpp)

    link_directories(/usr/lib/aarch64-linux-gnu/tegra)

    find_library(NVARGUS_LIBRARY nvargus HINTS /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/tegra)
    find_library(NVARGUS_SOCKETCLIENT_LIBRARY nvargus_socketclient HINTS /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/tegra)
    find_library(NVBUF_SURFACE_LIBRARY nvbufsurface HINTS /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/tegra)

    set(JETSON_LIBS
        ${NVARGUS_LIBRARY}
        ${NVARGUS_SOCKETCLIENT_LIBRARY}
        ${NVBUF_SURFACE_LIBRARY}

    )

    message(STATUS "Use libargus!")
elseif(${PLATFORM} STREQUAL "raspberrypi")
    pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera)
    list(APPEND CAPTURE_FILES libcamera_capturer.cpp)

    message(STATUS "libcamera library found:")
    message(STATUS "    version: ${LIBCAMERA_VERSION}")
    message(STATUS "    libraries: ${LIBCAMERA_LINK_LIBRARIES}")
    message(STATUS "    include path: ${LIBCAMERA_INCLUDE_DIRS}")
endif()

# PulseAudio
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
pkg_check_modules(PULSE_SIMPLE REQUIRED IMPORTED_TARGET libpulse-simple)
message(STATUS "PulseAudio library found:")
message(STATUS "    PULSE libs: ${PULSE_LINK_LIBRARIES}")
message(STATUS "    PULSE_SIMPLE libs: ${PULSE_SIMPLE_LINK_LIBRARIES}")

# Create target first
add_library(${PROJECT_NAME} ${CAPTURE_FILES})

# Include Paths
target_include_directories(${PROJECT_NAME} PUBLIC
    ${PULSE_INCLUDE_DIRS}
    ${PULSE_SIMPLE_INCLUDE_DIRS}
)

if(${PLATFORM} STREQUAL "jetson")
    target_include_directories(${PROJECT_NAME} PUBLIC
        /usr/src/jetson_multimedia_api/include
        /usr/src/jetson_multimedia_api/samples/common/classes
    )
endif()

if(DEFINED LIBCAMERA_INCLUDE_DIRS)
    target_include_directories(${PROJECT_NAME} PUBLIC ${LIBCAMERA_INCLUDE_DIRS})
endif()

# Link Libraries
target_link_libraries(${PROJECT_NAME} PUBLIC
    common
    v4l2_codecs
    ${WEBRTC_LIBRARY}
    ${PULSE_LINK_LIBRARIES}
    ${PULSE_SIMPLE_LINK_LIBRARIES}
)

if(DEFINED LIBCAMERA_LINK_LIBRARIES)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBCAMERA_LINK_LIBRARIES})
endif()

if(${PLATFORM} STREQUAL "jetson")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${JETSON_LIBS})
endif()
