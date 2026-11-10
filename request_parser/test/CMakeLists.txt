cmake_minimum_required(VERSION 3.12)
project(request_parser_tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)

set(REQUEST_PARSER_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

file(GLOB_RECURSE REQUEST_PARSER_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(request_parser_tests ${REQUEST_PARSER_TEST_SOURCES})

target_compile_definitions(request_parser_tests PRIVATE UNIT_TEST)

target_link_libraries(request_parser_tests
    PRIVATE
        request_parser
        protobuf_test_shutdown
        GTest::gtest
        GTest::gmock_main
)

target_include_directories(request_parser_tests
    PRIVATE
        ${REQUEST_PARSER_HEADERS_DIR}
)

add_test(NAME request_parser_tests COMMAND request_parser_tests)
