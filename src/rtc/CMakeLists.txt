project(rtc)

find_package(Protobuf REQUIRED)

# enable proto3 optional or not
set(PROTO_OPTIONAL_FLAG "")
if(Protobuf_PROTOC_EXECUTABLE)
  execute_process(
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} --version
    OUTPUT_VARIABLE PROTOC_VERSION_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" PROTOC_VERSION ${PROTOC_VERSION_RAW})
  message(STATUS "Found protoc version: ${PROTOC_VERSION}")

  if(PROTOC_VERSION VERSION_GREATER_EQUAL "3.12.0")
    set(PROTO_OPTIONAL_FLAG "--experimental_allow_proto3_optional")
    message(STATUS "Enabled proto3 optional support.")
  endif()
endif()

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
    ARGS ${PROTO_OPTIONAL_FLAG}
    --cpp_out=${PROTO_OUT_DIR}
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
