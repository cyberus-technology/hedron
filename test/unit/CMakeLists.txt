message(STATUS "Building tests: Check the README file for instructions on disabling them")
find_package(Catch2 REQUIRED)

add_executable(test_unit
  atomic.cpp main.cpp math.cpp mtrr.cpp page_table.cpp static_vector.cpp string.cpp
  )
target_link_libraries(test_unit Catch2::Catch2)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(test_unit PRIVATE -fsanitize=address -fsanitize=undefined)
  target_link_options(test_unit PRIVATE -fsanitize=address -fsanitize=undefined)

  # At least some GCC versions get their linking wrong with ASAN and produce the following linking warning:
  #
  # ASan runtime does not come first in initial library list; you
  # should either link runtime to your application or manually preload
  # it with LD_PRELOAD.
  #
  # Workaround by linking sanitizers statically.
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      target_link_options(test_unit PRIVATE -static-libasan -static-libubsan)
  endif()
endif()

include(Catch)
catch_discover_tests(test_unit)

if(COVERAGE)

  include(CodeCoverage)
  APPEND_COVERAGE_COMPILER_FLAGS()

  SETUP_TARGET_FOR_COVERAGE_GCOVR_HTML(
    NAME test_unit_coverage
    EXECUTABLE test_unit
    DEPENDENCIES test_unit)

endif()
