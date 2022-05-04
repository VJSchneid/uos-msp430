add_library(uos OBJECT
  scheduler.cpp
  thread.cpp
  thread.s
  timer.cpp
)

target_link_libraries(uos PUBLIC config)