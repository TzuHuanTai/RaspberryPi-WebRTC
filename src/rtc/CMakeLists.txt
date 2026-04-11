project(rtc)

# Use protoc matching WebRTC's bundled protobuf (v33) to avoid version conflicts
find_program(PROTOC_EXECUTABLE protoc)
if(NOT PROTOC_EXECUTABLE)
  message(FATAL_ERROR "protoc not found. Install protoc v33 matching WebRTC's bundled protobuf.")
endif()
execute_process(
  COMMAND ${PROTOC_EXECUTABLE} --version
  OUTPUT_VARIABLE PROTOC_VERSION_RAW
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX MATCH "[0-9]+\\.[0-9]+" PROTOC_VERSION "${PROTOC_VERSION_RAW}")
message(STATUS "Using protoc: ${PROTOC_EXECUTABLE} (${PROTOC_VERSION_RAW})")
if(PROTOC_VERSION VERSION_LESS "33.0")
  message(FATAL_ERROR "protoc >= 33.0 is required to match WebRTC's bundled protobuf, found ${PROTOC_VERSION}")
endif()

# well-known proto include path (e.g. google/protobuf/empty.proto)
get_filename_component(PROTOC_DIR ${PROTOC_EXECUTABLE} DIRECTORY)
get_filename_component(PROTOC_PREFIX ${PROTOC_DIR} DIRECTORY)
set(PROTOC_INCLUDE_DIR "${PROTOC_PREFIX}/include")

# path of the proto files
set(PROTO_SRC_DIRS
  "${CMAKE_SOURCE_DIR}/external/livekit-protocol/protobufs"
  "${CMAKE_SOURCE_DIR}/external/protocol/protos"
)
set(PROTO_OUT_DIR "${CMAKE_SOURCE_DIR}/proto")
file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

set(PROTO_SRCS "")

# Generate well-known types not included in libwebrtc.a (Timestamp, Duration)
set(WELL_KNOWN_PROTOS timestamp duration)
foreach(wk_name ${WELL_KNOWN_PROTOS})
  set(wk_proto "${PROTOC_INCLUDE_DIR}/google/protobuf/${wk_name}.proto")
  set(wk_cc "${PROTO_OUT_DIR}/google/protobuf/${wk_name}.pb.cc")
  set(wk_h "${PROTO_OUT_DIR}/google/protobuf/${wk_name}.pb.h")
  add_custom_command(
    OUTPUT ${wk_cc} ${wk_h}
    COMMAND ${PROTOC_EXECUTABLE}
    ARGS --cpp_out=lite:${PROTO_OUT_DIR}
    -I ${PROTOC_INCLUDE_DIR}
    ${wk_proto}
    DEPENDS ${wk_proto}
  )
  list(APPEND PROTO_SRCS ${wk_cc})
endforeach()

foreach(PROTO_SRC_DIR ${PROTO_SRC_DIRS})
  # collect all .proto
  file(GLOB PROTO_FILES "${PROTO_SRC_DIR}/*.proto")

  # generate `*.pb.cc` and `*.pb.h` to `proto/`
  foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME_WE)

    set(proto_cc "${PROTO_OUT_DIR}/${proto_name}.pb.cc")
    set(proto_h "${PROTO_OUT_DIR}/${proto_name}.pb.h")

    add_custom_command(
      OUTPUT ${proto_cc} ${proto_h}
      COMMAND ${PROTOC_EXECUTABLE}
      ARGS --cpp_out=lite:${PROTO_OUT_DIR}
      -I ${PROTO_SRC_DIR}
      -I ${PROTOC_INCLUDE_DIR}
      ${proto_file}
      DEPENDS ${proto_file}
    )

    list(APPEND PROTO_SRCS ${proto_cc})
  endforeach()
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
  track
  ipc
  ${WEBRTC_LIBRARY}
)

if(USE_JETSON_HW_ENCODER)
  target_link_libraries(${PROJECT_NAME} PUBLIC
    jetson_codecs
  )
endif()

if(USE_RPI_HW_ENCODER)
  target_link_libraries(${PROJECT_NAME} PUBLIC
    v4l2_codecs
  )
endif()
