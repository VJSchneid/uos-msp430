if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "msp430")

  add_executable(stable-sensor main.cpp)

  target_link_libraries(stable-sensor PUBLIC config uos_device_scheduler uos_device_gpio uos_device_timer_a0)
endif()
