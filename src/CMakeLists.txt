project(src)

add_subdirectory(common)
add_subdirectory(track)
add_subdirectory(capturer)
add_subdirectory(codecs/h264)
add_subdirectory(codecs/v4l2)
add_subdirectory(signaling)
add_subdirectory(recorder)
add_subdirectory(rtc)

add_library(${PROJECT_NAME}
    parser.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC rtc signaling recorder common)
