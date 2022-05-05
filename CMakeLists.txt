cmake_minimum_required(VERSION 3.12)

project(reflow-controller)
enable_language(ASM)



add_library(config INTERFACE)

target_compile_features(config INTERFACE cxx_std_20)
target_compile_options(config INTERFACE "-g")

target_include_directories(config INTERFACE "include/")

message(STATUS ${CMAKE_SYSTEM_PROCESSOR})

add_subdirectory(src)


execute_process(
  COMMAND git describe --always --dirty
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_compile_definitions(config INTERFACE GIT_VERSION="${GIT_VERSION}")