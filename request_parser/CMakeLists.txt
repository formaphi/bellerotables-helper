cmake_minimum_required(VERSION 3.20)
project(request_parser CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(protobuf CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)

set(REQUEST_PARSER_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/include")
set(REQUEST_PARSER_SOURCES_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

message("[request_parser] path to headers: ${REQUEST_PARSER_HEADERS_DIR}")
message("[request_parser] path to sources: ${REQUEST_PARSER_SOURCES_DIR}")

file(GLOB_RECURSE REQUEST_PARSER_HEADERS "${REQUEST_PARSER_HEADERS_DIR}/*.h")
file(GLOB_RECURSE REQUEST_PARSER_SOURCES "${REQUEST_PARSER_SOURCES_DIR}/*.cpp")

add_library(request_parser
    STATIC
        ${REQUEST_PARSER_SOURCES}
        ${REQUEST_PARSER_HEADERS}
)

target_include_directories(request_parser
    PUBLIC
        $<BUILD_INTERFACE:${REQUEST_PARSER_HEADERS_DIR}>
)

# LEARN: domain is PUBLIC because IRequestParser returns domain::RedirectionRequest. PRIVATE keeps
# protobuf, nlohmann and the generated contract headers off every consumer's include path
# (docs/request-controller.md: "Protobuf stays inside the parser library"). Consumers still link
# them, because this is a static library, but cannot #include them.
target_link_libraries(request_parser
    PUBLIC
        domain
    PRIVATE
        contracts_cpp
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf
)

set_target_properties(request_parser
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS request_parser)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
