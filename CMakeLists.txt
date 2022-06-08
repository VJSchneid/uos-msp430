cmake_minimum_required(VERSION 3.13)

project(reflow-controller)
enable_language(ASM)


add_library(config INTERFACE)

target_compile_features(config INTERFACE cxx_std_20)
target_compile_options(config INTERFACE "-g" "-Wall")

target_include_directories(config INTERFACE "include/")
target_compile_definitions(config INTERFACE UOS_ARCH_$<UPPER_CASE:${CMAKE_SYSTEM_PROCESSOR}>)

# msp430 specific configuration (for main example)
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "msp430")

  target_compile_definitions(config INTERFACE
    UOS_NUMBER_OF_TASKS=3
    UOS_DEV_MSP430_ENABLE_PORT2
    UOS_DEV_MSP430_ENABLE_TIMERA0
    UOS_DEV_MSP430_ENABLE_EUSCIA1
  )

endif()

add_subdirectory(src)

# build tests when not cross compiling
if(NOT ${CMAKE_CROSSCOMPILING})
  add_subdirectory(test)
endif()


# git version information
execute_process(
  COMMAND git describe --always --dirty
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_compile_definitions(config INTERFACE GIT_VERSION="${GIT_VERSION}")