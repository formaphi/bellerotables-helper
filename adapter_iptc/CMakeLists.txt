cmake_minimum_required(VERSION 3.15)
project(adapter_iptc CXX)

# Set C++ standard consistent with project
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set directories
set(ADAPTER_IPTC_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/include")
set(ADAPTER_IPTC_SOURCES_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

message("[adapter_iptc] path to headers: ${ADAPTER_IPTC_HEADERS_DIR}")
message("[adapter_iptc] path to sources: ${ADAPTER_IPTC_SOURCES_DIR}")

# Globbing files
file(GLOB_RECURSE ADAPTER_IPTC_HEADERS "${ADAPTER_IPTC_HEADERS_DIR}/*.h")
file(GLOB_RECURSE ADAPTER_IPTC_SOURCES "${ADAPTER_IPTC_SOURCES_DIR}/*.cpp")

# Create library
add_library(adapter_iptc
    STATIC
        ${ADAPTER_IPTC_SOURCES}
        ${ADAPTER_IPTC_HEADERS}
)

# Include directories
target_include_directories(adapter_iptc
    PUBLIC
        $<BUILD_INTERFACE:${ADAPTER_IPTC_HEADERS_DIR}>
)

# Link libraries
target_link_libraries(adapter_iptc
    PUBLIC
      ip4tc
)

# Set position-independent-code for static libs used in shared contexts
target_compile_options(adapter_iptc PRIVATE -fpermissive)

set_target_properties(adapter_iptc
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Export target for parent projects
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS adapter_iptc)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
