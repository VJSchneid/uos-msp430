add_subdirectory(uos)

add_executable(spi
    spi.cpp)

add_executable(main
    main.cpp)

target_link_libraries(main PUBLIC config uos_device_msp430)
