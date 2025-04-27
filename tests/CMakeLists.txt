add_executable(test_logger
    test_logger.cc
)

set_target_properties(test_logger PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_ROOT}/tests
)

target_include_directories(test_logger PRIVATE ${PROJECT_SOURCE_DIR}/src)
find_package(Threads REQUIRED)
target_link_libraries(test_logger PRIVATE Threads::Threads)