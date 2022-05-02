cmake_minimum_required(VERSION 3.21)
project(vkr0)

set(CMAKE_CXX_STANDARD 20)

add_executable(vkr0 main.cpp)
target_link_libraries(vkr0 z)
