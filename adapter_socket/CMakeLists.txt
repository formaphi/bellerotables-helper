cmake_minimum_required(VERSION 3.20)
project(adapter_socket CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ADAPTER_SOCKET_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/include")
set(ADAPTER_SOCKET_SOURCES_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

message("[adapter_socket] path to headers: ${ADAPTER_SOCKET_HEADERS_DIR}")
message("[adapter_socket] path to sources: ${ADAPTER_SOCKET_SOURCES_DIR}")

file(GLOB_RECURSE ADAPTER_SOCKET_HEADERS "${ADAPTER_SOCKET_HEADERS_DIR}/*.h")
file(GLOB_RECURSE ADAPTER_SOCKET_SOURCES "${ADAPTER_SOCKET_SOURCES_DIR}/*.cpp")

add_library(adapter_socket
    STATIC
        ${ADAPTER_SOCKET_SOURCES}
        ${ADAPTER_SOCKET_HEADERS}
)

target_include_directories(adapter_socket
    PUBLIC
        $<BUILD_INTERFACE:${ADAPTER_SOCKET_HEADERS_DIR}>
)

set_target_properties(adapter_socket
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS adapter_socket)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
