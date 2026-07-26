add_library(bootloader_core STATIC 
    src/syscalls.c
    src/system_stm32wbxx.c
    src/sysmem.c
)

target_include_directories(bootloader_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_compile_definitions(bootloader_core PUBLIC 
    STM32WB55xx 
)