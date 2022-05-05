add_library(uos OBJECT
  scheduler.cpp
  thread.cpp
  thread.s
  timer.cpp
  pin2.cpp
  uca1.cpp
)

target_link_libraries(uos PUBLIC config)