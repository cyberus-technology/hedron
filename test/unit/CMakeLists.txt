find_package(Catch2 QUIET)

if(Catch2_FOUND)

  add_executable(test_unit
    atomic.cpp main.cpp math.cpp
    )
  target_link_libraries(test_unit Catch2::Catch2)

  include(CTest)
  include(Catch)
  catch_discover_tests(test_unit)

else()

  message(STATUS "Catch2 not found. Unit tests will not be built.")

endif()
