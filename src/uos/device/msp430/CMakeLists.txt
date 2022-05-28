add_library(uos_device OBJECT
  scheduler.cpp
  scheduler.s
  gpio.cpp
  timer_a.cpp
)

target_link_libraries(uos_device PUBLIC config)