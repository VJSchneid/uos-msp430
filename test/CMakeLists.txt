cmake_policy(SET CMP0076 NEW)

find_package(Catch2 REQUIRED)

add_executable(uos_test
  main.cpp
  basic_scheduler.cpp
)

add_subdirectory(detail)
add_subdirectory(device)

target_link_libraries(uos_test PRIVATE config Catch2::Catch2)

include(CTest)
include(Catch)
catch_discover_tests(uos_test)