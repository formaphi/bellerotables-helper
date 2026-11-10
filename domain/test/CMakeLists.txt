cmake_minimum_required(VERSION 3.12)
project(domain_tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)

set(DOMAIN_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

file(GLOB_RECURSE DOMAIN_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(domain_tests ${DOMAIN_TEST_SOURCES})

target_compile_definitions(domain_tests PRIVATE UNIT_TEST)

target_link_libraries(domain_tests
    PRIVATE
        domain
        GTest::gtest
        GTest::gmock_main
)

target_include_directories(domain_tests
    PRIVATE
        ${DOMAIN_HEADERS_DIR}
)

add_test(NAME domain_tests COMMAND domain_tests)
