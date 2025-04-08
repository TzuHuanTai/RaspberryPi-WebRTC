project(common)

find_package(JPEG REQUIRED)
include_directories(${JPEG_INCLUDE_DIR})

aux_source_directory(${PROJECT_SOURCE_DIR} COMMON_FILES)

add_library(${PROJECT_NAME} ${COMMON_FILES})

# Use libyuv in libwebrtc.a
target_link_libraries(${PROJECT_NAME}
    avutil
    avformat
    uuid
    ${JPEG_LIBRARIES}
    ${WEBRTC_LIBRARY}
)
