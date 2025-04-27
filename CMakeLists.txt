cmake_minimum_required(VERSION 3.15)
project(CloudStorageSystem)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${PROJECT_SOURCE_DIR}/src)

# 定义统一的bin目录变量
set(BIN_OUTPUT_ROOT ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(tests)