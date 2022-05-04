cmake_minimum_required(VERSION 3.12)

project(reflow-controller)
enable_language(ASM)



add_library(config INTERFACE)

target_compile_features(config INTERFACE cxx_std_20)
target_compile_options(config INTERFACE "-g")

target_include_directories(config INTERFACE "include/")

message(STATUS ${CMAKE_SYSTEM_PROCESSOR})

add_subdirectory(src)
