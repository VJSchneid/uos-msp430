if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "msp430")

  add_executable(reflow-controller main.cpp)

  target_link_libraries(reflow-controller PUBLIC config uos_device_scheduler uos_device_gpio uos_device_timer_a0 uos_device_eusci)
endif()
