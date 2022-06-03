find_package(Catch2 REQUIRED)

add_executable(uos_test
  main.cpp
)

target_link_libraries(uos_test PRIVATE Catch2::Catch2)

include(CTest)
include(Catch)
catch_discover_tests(uos_test)