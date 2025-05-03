cmake_minimum_required(VERSION 3.16)
project(CCcloud)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Boost REQUIRED)
include_directories(${PROJECT_SOURCE_DIR}/src)
set(PROTO_SRC
    src/generated/file.pb.cc
    src/generated/file.grpc.pb.cc
)

set(BIN_OUTPUT_ROOT ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(tests)

# ----------------- Server -----------------
add_executable(naive_server
    src/server/naive_server.cc
    ${PROTO_SRC}
)

target_link_libraries(naive_server
    gRPC::grpc++
    protobuf::libprotobuf
    Boost::headers
)

add_executable(CCcloud_server
    src/server/server.cc
    src/server/AsyncCall.hpp
    ${PROTO_SRC}
)

target_link_libraries(CCcloud_server
    gRPC::grpc++
    protobuf::libprotobuf
    Boost::headers
)

# ----------------- Client -----------------
add_executable(naive_client
    src/client/naive_client.cc
    ${PROTO_SRC}
)

target_link_libraries(naive_client
    gRPC::grpc++
    protobuf::libprotobuf
    Boost::headers
)

add_executable(CCcloud_client
    src/client/async_client.cc
    ${PROTO_SRC}
)

target_link_libraries(CCcloud_client
    gRPC::grpc++
    protobuf::libprotobuf
    Boost::headers
)

# ----------------- Options -----------------

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer ENABLE_ASAN option is ON. Adding sanitizer flags.")

    # 为 CCcloud_server target 添加编译选项
    # 这些标志对于 g++ (>= 4.8) 和 Clang 都有效
    target_compile_options(CCcloud_server PRIVATE
        -fsanitize=address
        -fno-omit-frame-pointer
        -g
    )

    # 为 CCcloud_server target 添加链接选项
    target_link_options(CCcloud_server PRIVATE
        -fsanitize=address
    )

    message(STATUS "AddressSanitizer flags added to CCcloud_server.")
endif()