add_library(uos_device_scheduler OBJECT scheduler.cpp scheduler.s)
target_link_libraries(uos_device_scheduler PUBLIC config)

add_library(uos_device_gpio OBJECT gpio.cpp)
target_link_libraries(uos_device_gpio PUBLIC config)

add_library(uos_device_eusci OBJECT eusci.cpp)
target_link_libraries(uos_device_eusci PUBLIC config)

add_library(uos_device_timer_a0 OBJECT timer_a0.cpp)
target_link_libraries(uos_device_timer_a0 PUBLIC config)