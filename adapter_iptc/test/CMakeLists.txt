cmake_minimum_required(VERSION 3.12)
project(adapter_iptc_tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)

set(ADAPTER_IPTC_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

file(GLOB_RECURSE ADAPTER_IPTC_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(adapter_iptc_tests ${ADAPTER_IPTC_TEST_SOURCES})

target_compile_definitions(adapter_iptc_tests PRIVATE UNIT_TEST)

target_compile_options(adapter_iptc_tests PRIVATE -fpermissive)

target_link_libraries(adapter_iptc_tests
    PRIVATE
        adapter_iptc
        GTest::gtest
        GTest::gmock_main
)

target_include_directories(adapter_iptc_tests
    PRIVATE
        ${ADAPTER_IPTC_HEADERS_DIR}
)

add_test(NAME adapter_iptc_tests COMMAND adapter_iptc_tests)
