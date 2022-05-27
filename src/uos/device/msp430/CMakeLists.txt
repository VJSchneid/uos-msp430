add_library(uos_device OBJECT
  scheduler.cpp
  scheduler.s
)

target_link_libraries(uos_device PUBLIC config)