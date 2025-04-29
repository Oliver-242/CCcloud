#!/bin/bash

PROTOC=/home/olivercai/vcpkg/installed/x64-linux/tools/protobuf/protoc
GRPC_CPP_PLUGIN=/home/olivercai/vcpkg/installed/x64-linux/tools/grpc/grpc_cpp_plugin

SRC_DIR=src/proto
OUT_DIR=src/generated

if [ ! -f "$PROTOC" ]; then
    echo "❌ Error: protoc not found at $PROTOC"
    exit 1
fi

if [ ! -f "$GRPC_CPP_PLUGIN" ]; then
    echo "❌ Error: grpc_cpp_plugin not found at $GRPC_CPP_PLUGIN"
    exit 1
fi

if [ ! -d "$SRC_DIR" ]; then
    echo "❌ Error: Proto source directory not found: $SRC_DIR"
    exit 1
fi

# 创建输出目录（如果不存在）
mkdir -p "$OUT_DIR"

echo "🚀 Generating proto files from $SRC_DIR to $OUT_DIR ..."

$PROTOC -I"$SRC_DIR" \
  --cpp_out="$OUT_DIR" \
  --grpc_out="$OUT_DIR" \
  --plugin=protoc-gen-grpc="$GRPC_CPP_PLUGIN" \
  "$SRC_DIR/file.proto"

if [ $? -eq 0 ]; then
    echo "✅ Proto generation successful!"
else
    echo "❌ Proto generation failed!"
    exit 2
fi
