message(STATUS "Building tests: Check the README file for instructions on disabling them")
find_package(Catch2 REQUIRED)

add_executable(test_unit
  acpi_fadt.cpp
  algorithm.cpp
  atomic.cpp
  bitmap.cpp
  list.cpp
  main.cpp
  math.cpp
  mtrr.cpp
  page_table.cpp
  static_vector.cpp
  string.cpp
  unique_ptr.cpp
  vmx_msr_bitmap.cpp
  vmx_preemption_timer.cpp
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

# We consciously don't use catch_discover_tests to auto-discover
# tests, because this results in no tests being run and success being
# reported, when the test binary is not able to list its tests.
add_test(NAME combined_unit_test COMMAND test_unit)

if(COVERAGE)

  include(CodeCoverage)
  APPEND_COVERAGE_COMPILER_FLAGS()

  SETUP_TARGET_FOR_COVERAGE_GCOVR_HTML(
    NAME test_unit_coverage
    EXECUTABLE test_unit
    DEPENDENCIES test_unit)

endif()
