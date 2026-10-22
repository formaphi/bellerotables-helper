cmake_minimum_required(VERSION 3.28)

project(
    bellerotables-helper
    DESCRIPTION "One-shot, socket-activated iptables helper for Bellerophon"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

option(ENABLE_COVERAGE "Enable gcov coverage instrumentation" OFF)

if(ENABLE_COVERAGE AND NOT BUILD_TESTING)
    message(WARNING "ENABLE_COVERAGE=ON has no effect when BUILD_TESTING=OFF.")
endif()

if(ENABLE_COVERAGE AND BUILD_TESTING)
    message(STATUS "Coverage instrumentation enabled")
    string(APPEND CMAKE_CXX_FLAGS " --coverage")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " --coverage")
endif()

add_executable(iptables-manager main.cpp)
