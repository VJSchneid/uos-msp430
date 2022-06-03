# this file decides which architecture subdirectory to choose

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "msp430")
  add_subdirectory(msp430)
else()
  add_library(uos_device INTERFACE)
endif()