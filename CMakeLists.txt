cmake_minimum_required(VERSION 3.10)
project(iptables-manager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(iptables-manager main.cpp MangleAdapter.cpp LibiptcHandle.cpp)

target_compile_options(iptables-manager PRIVATE -fpermissive)
target_link_libraries(iptables-manager PRIVATE ip4tc)
