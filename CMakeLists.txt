cmake_minimum_required(VERSION 3.13)
project(NOVA CXX ASM)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)

add_subdirectory(src)

enable_testing()
add_subdirectory(test/unit)
