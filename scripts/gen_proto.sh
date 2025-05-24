#!/bin/bash

PROTO_SRC_DIR=external/livekit-protocol/protobufs
PROTO_OUT_DIR=proto

mkdir -p $PROTO_OUT_DIR

protoc -I=$PROTO_SRC_DIR --cpp_out=$PROTO_OUT_DIR $PROTO_SRC_DIR/*.proto
