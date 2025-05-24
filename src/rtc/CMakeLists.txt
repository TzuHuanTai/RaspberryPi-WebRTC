project(rtc)

find_package(Protobuf REQUIRED)

# path of the proto files
set(PROTO_SRC_DIR "${CMAKE_SOURCE_DIR}/external/livekit-protocol/protobufs")
set(PROTO_OUT_DIR "${CMAKE_SOURCE_DIR}/proto")

file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

# collect all .proto
file(GLOB PROTO_FILES "${PROTO_SRC_DIR}/*.proto")

# generate `*.pb.cc` and `*.pb.h` to `proto/`
foreach(proto_file ${PROTO_FILES})
  get_filename_component(proto_name ${proto_file} NAME_WE)

  set(proto_cc "${PROTO_OUT_DIR}/${proto_name}.pb.cc")
  set(proto_h "${PROTO_OUT_DIR}/${proto_name}.pb.h")

  add_custom_command(
    OUTPUT ${proto_cc} ${proto_h}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${PROTO_OUT_DIR}
    -I ${PROTO_SRC_DIR}
    ${proto_file}
    DEPENDS ${proto_file}
  )

  list(APPEND PROTO_SRCS ${proto_cc})
  list(APPEND PROTO_HDRS ${proto_h})
endforeach()

aux_source_directory(${PROJECT_SOURCE_DIR} RTC_FILES)

add_library(${PROJECT_NAME}
  ${RTC_FILES}
  ${PROTO_SRCS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${PROTO_OUT_DIR}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  common
  track
  capturer
  v4l2_codecs
  ipc
  protobuf::libprotobuf
)
