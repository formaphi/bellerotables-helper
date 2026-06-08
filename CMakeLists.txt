
cmake_minimum_required(VERSION 3.15)
project(iptables-manager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PostgreSQL REQUIRED)

add_executable(iptables-manager main.cpp)
target_link_libraries(iptables-manager PRIVATE PostgreSQL::PostgreSQL)
