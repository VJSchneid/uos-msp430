set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR msp430)

# can be set if needed:
set(CMAKE_SYSROOT /usr/msp430-elf/)
#set(CMAKE_STAGING_PREFIX /home/devel/stage)

set(CMAKE_C_COMPILER msp430-elf-gcc)
set(CMAKE_CXX_COMPILER msp430-elf-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_CXX_FLAGS "-mmcu=msp430fr2433")
set(CMAKE_C_FLAGS "-mmcu=msp430fr2433")
set(CMAKE_LINKER_FLAGS "-mmcu=msp430fr2433")
