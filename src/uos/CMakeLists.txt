add_library(uos OBJECT
  scheduler.cpp
  timer.cpp
  pin2.cpp
  uca1.cpp
)

add_subdirectory(device)

target_link_libraries(uos PUBLIC config)