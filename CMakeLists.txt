cmake_minimum_required(VERSION 3.13)
project(NOVA CXX ASM)

include(CTest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(default_build_type "Debug")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release")
endif()

# Globally enable additional goodies, if we are compiling with clang.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "Debug")

  # Optionally enable clang-tidy for the whole project.
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")

  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  else()
    message(STATUS "clang-tidy not found. Please install for automatic static analysis.")
  endif()

endif()

include_directories(include)

add_subdirectory(src)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test/unit)
endif()
