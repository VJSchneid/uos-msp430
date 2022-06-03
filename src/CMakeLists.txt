add_subdirectory(uos)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "msp430")
    add_executable(main
        main.cpp
    )

    target_link_libraries(main PUBLIC config uos_device)
endif()
