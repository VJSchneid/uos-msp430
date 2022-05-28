add_library(uos_device_msp430 OBJECT
  scheduler.cpp
  scheduler.s
  gpio.cpp
  timer_a.cpp
  eusci.cpp
)

target_link_libraries(uos_device_msp430 PUBLIC config)