find_package(Catch2 QUIET)

if(Catch2_FOUND)

  add_executable(test_unit
    atomic.cpp main.cpp math.cpp
    )
  target_link_libraries(test_unit Catch2::Catch2)

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
