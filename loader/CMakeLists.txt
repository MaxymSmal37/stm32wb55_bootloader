cmake_minimum_required(VERSION 3.16)
project(stm32wb55_loader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(stm32wb55_loader
    bootloader_cli.c
)

target_compile_options(stm32wb55_loader PRIVATE
    -O2 -Wall -Wextra
)

install(TARGETS stm32wb55_loader RUNTIME DESTINATION bin)
