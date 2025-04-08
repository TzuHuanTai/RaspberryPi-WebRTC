project(recorder)

set(FFMPEG_LINK_LIBS avformat)

aux_source_directory(${PROJECT_SOURCE_DIR} RECORDER_FILES)

add_library(${PROJECT_NAME} ${RECORDER_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC capturer v4l2_codecs h264_codecs ${FFMPEG_LINK_LIBS})
