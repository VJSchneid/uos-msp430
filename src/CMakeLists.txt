
add_executable(spi
    spi.cpp)

add_executable(main
    thread.s
    thread.cpp
    scheduler.cpp
    timer.cpp
    main.cpp)

target_link_libraries(main PUBLIC config)
