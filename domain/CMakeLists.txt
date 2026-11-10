cmake_minimum_required(VERSION 3.20)
project(domain CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DOMAIN_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/include")

message("[domain] path to headers: ${DOMAIN_HEADERS_DIR}")

file(GLOB_RECURSE DOMAIN_HEADERS "${DOMAIN_HEADERS_DIR}/*.h")

# LEARN: header-only. The domain types are plain aggregates with nothing to compile, and the
# library depends on nothing and links nothing (docs/request-controller.md, Ring 2).
add_library(domain INTERFACE ${DOMAIN_HEADERS})

target_include_directories(domain
    INTERFACE
        $<BUILD_INTERFACE:${DOMAIN_HEADERS_DIR}>
)

target_compile_features(domain INTERFACE cxx_std_23)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS domain)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
