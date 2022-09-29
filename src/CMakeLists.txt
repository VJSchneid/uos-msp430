add_subdirectory(uos)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "msp430")
    add_executable(main
        main.cpp
    )

    target_link_libraries(main PUBLIC config uos_device_eusci uos_device_scheduler uos_device_gpio uos_device_timer_a)
endif()
