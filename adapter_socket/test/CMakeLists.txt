cmake_minimum_required(VERSION 3.12)
project(adapter_socket_tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)

set(ADAPTER_SOCKET_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

file(GLOB_RECURSE ADAPTER_SOCKET_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(adapter_socket_tests ${ADAPTER_SOCKET_TEST_SOURCES})

target_compile_definitions(adapter_socket_tests PRIVATE UNIT_TEST)

target_link_libraries(adapter_socket_tests
    PRIVATE
        adapter_socket
        GTest::gtest
        GTest::gmock_main
)

target_include_directories(adapter_socket_tests
    PRIVATE
        ${ADAPTER_SOCKET_HEADERS_DIR}
)

add_test(NAME adapter_socket_tests COMMAND adapter_socket_tests)
