project(rtc)

aux_source_directory(${PROJECT_SOURCE_DIR} RTC_FILES)

add_library(${PROJECT_NAME} ${RTC_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC common track capturer v4l2_codecs ipc)
