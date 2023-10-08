cmake_minimum_required(VERSION 3.13)

project(HEDRON CXX ASM)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(default_build_type "Debug")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release")
endif()

option(ENABLE_CLANG_TIDY "Enable clang-tidy analysis." OFF)

# Enable clang-tidy checks if user enables it and we have a clang toolchain
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXECUTABLE clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR})
endif()

include_directories(include)

add_subdirectory(src)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test/unit)
endif()
