cmake_minimum_required(VERSION 3.12)
project(adapter_iptc_tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)

set(ADAPTER_IPTC_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

file(GLOB_RECURSE ADAPTER_IPTC_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
list(FILTER ADAPTER_IPTC_TEST_SOURCES EXCLUDE REGEX "/kernel/")

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

# LEARN: the kernel tier drives the real IptcHelper against the kernel, no mocks. Its main() moves
# the process into a new network namespace first, so it never touches the host's tables; where
# that is not allowed every test skips (GTEST_SKIP) instead of failing. Label: kernel.
file(GLOB ADAPTER_IPTC_KERNEL_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/kernel/*.cpp")

add_executable(adapter_iptc_kernel_tests ${ADAPTER_IPTC_KERNEL_TEST_SOURCES})

target_compile_options(adapter_iptc_kernel_tests PRIVATE -fpermissive)

target_link_libraries(adapter_iptc_kernel_tests
    PRIVATE
        adapter_iptc
        GTest::gtest
)

target_include_directories(adapter_iptc_kernel_tests
    PRIVATE
        ${ADAPTER_IPTC_HEADERS_DIR}
        ${CMAKE_CURRENT_LIST_DIR}
)

add_test(NAME adapter_iptc_kernel_tests COMMAND adapter_iptc_kernel_tests)
set_tests_properties(adapter_iptc_kernel_tests PROPERTIES LABELS kernel)
