cmake_minimum_required(VERSION 3.16)

# If the user did not pass a toolchain file, default to the bundled
# arm-none-eabi one so a plain `cmake ..` just works.
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake"
        CACHE FILEPATH "Toolchain file")
endif()

project(STM32wb55_Bootloader C CXX ASM)

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

option(BUILD_HOST_LOADER "Build the host-side serial loader for PC flashing" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ----------------------------------------------------------------------------
# Target MCU: STM32WB55CGUx (Cortex-M4, soft float to match the startup file)
# ----------------------------------------------------------------------------
set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=soft
)

add_compile_options(
    ${MCU_FLAGS}
    -ffunction-sections      # place each function in its own section ...
    -fdata-sections          # ... and each datum, so --gc-sections can prune
    -Wall
    -Wextra
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-Os>
)

add_compile_definitions(
    STM32WB55xx              # selects the right register map in the CMSIS headers
)

add_link_options(
    ${MCU_FLAGS}
    -Wl,--gc-sections        # drop unused sections
    --specs=nano.specs       # newlib-nano
    --specs=nosys.specs      # stub syscalls (_write, _sbrk, ...) for bare metal
    -Wl,--print-memory-usage
)

if(BUILD_HOST_LOADER)
    find_program(HOST_CC NAMES cc gcc clang REQUIRED)
    add_custom_target(stm32wb55_loader ALL
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/loader/build/stm32wb55_loader
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/loader/build
        COMMAND ${HOST_CC} -O2 -Wall -Wextra -o ${CMAKE_SOURCE_DIR}/loader/build/stm32wb55_loader ${CMAKE_SOURCE_DIR}/loader/bootloader_cli.c
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building host-side serial loader"
        VERBATIM
    )
endif()

add_subdirectory(core)
add_subdirectory(bootloader)
add_subdirectory(app)
add_subdirectory(communication)
