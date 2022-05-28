add_library(uos OBJECT
  uca1.cpp
)

add_subdirectory(device)

target_link_libraries(uos PUBLIC config)