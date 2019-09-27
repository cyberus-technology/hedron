find_package(Catch2 QUIET)

if(Catch2_FOUND)

  add_executable(test_unit
    atomic.cpp main.cpp math.cpp
    )
  target_link_libraries(test_unit Catch2::Catch2)

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test_unit PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(test_unit PRIVATE -fsanitize=address -fsanitize=undefined)
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

else()

  message(STATUS "Catch2 not found. Unit tests will not be built.")

endif()
