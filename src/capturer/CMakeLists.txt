project(capturer)

find_package(PkgConfig REQUIRED)

# Source files
set(CAPTURE_FILES
    v4l2_capturer.cpp
    pa_capturer.cpp
)

if(${PLATFORM} STREQUAL "jetson")
    set(MULTIMEDIA_API_BASE /usr/src/jetson_multimedia_api)
    set(TEGRA_LIB_DIR /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/tegra)

    list(APPEND CAPTURE_FILES
        libargus_egl_capturer.cpp
        libargus_buffer_capturer.cpp

        ${MULTIMEDIA_API_BASE}/samples/common/classes/NvBuffer.cpp
        ${MULTIMEDIA_API_BASE}/samples/common/classes/NvLogging.cpp
        ${MULTIMEDIA_API_BASE}/samples/common/classes/NvBufSurface.cpp

        ${MULTIMEDIA_API_BASE}/argus/samples/utils/Thread.cpp
        ${MULTIMEDIA_API_BASE}/argus/samples/utils/NativeBuffer.cpp
        ${MULTIMEDIA_API_BASE}/argus/samples/utils/nvmmapi/NvNativeBuffer.cpp
        ${MULTIMEDIA_API_BASE}/samples/common/classes/NvV4l2ElementPlane.cpp
        ${MULTIMEDIA_API_BASE}/samples/common/classes/NvVideoEncoder.cpp
        ${MULTIMEDIA_API_BASE}/samples/common/classes/NvElement.cpp
        ${MULTIMEDIA_API_BASE}/samples/common/classes/NvV4l2Element.cpp
        ${MULTIMEDIA_API_BASE}/samples/common/classes/NvElementProfiler.cpp
    )

    # Add multimedia API include directory
    link_directories(${TEGRA_LIB_DIR})

    # Find all required Jetson libraries
    find_library(NVARGUS_LIBRARY nvargus HINTS ${TEGRA_LIB_DIR})
    find_library(NVARGUS_SOCKETCLIENT_LIBRARY nvargus_socketclient HINTS ${TEGRA_LIB_DIR})
    find_library(NVBUF_SURFACE_LIBRARY nvbufsurface HINTS ${TEGRA_LIB_DIR})
    find_library(NVBUF_SURFACE_TRANSFORM_LIBRARY nvbufsurftransform HINTS ${TEGRA_LIB_DIR})
    find_library(NVV4L2_LIBRARY nvv4l2 HINTS ${TEGRA_LIB_DIR})
    find_library(EGL_LIBRARY EGL)

    set(JETSON_LIBS
        ${NVARGUS_LIBRARY}
        ${NVARGUS_SOCKETCLIENT_LIBRARY}
        ${NVBUF_SURFACE_LIBRARY}
        ${NVBUF_SURFACE_TRANSFORM_LIBRARY}
        ${NVV4L2_LIBRARY}
        ${EGL_LIBRARY}
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
        ${MULTIMEDIA_API_BASE}/include
        ${MULTIMEDIA_API_BASE}/samples/common/classes
        ${MULTIMEDIA_API_BASE}/argus/samples/utils
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
