#!/bin/bash

PROTO_OUT_DIR=proto
PROTO_DIRS=(
  external/livekit-protocol/protobufs
  external/protocol/protos
)

mkdir -p $PROTO_OUT_DIR

INCLUDES=""
FILES=""

for dir in "${PROTO_DIRS[@]}"; do
  INCLUDES="$INCLUDES -I=$dir"
  FILES="$FILES $dir/*.proto"
done

protoc $INCLUDES --cpp_out=$PROTO_OUT_DIR $FILES
