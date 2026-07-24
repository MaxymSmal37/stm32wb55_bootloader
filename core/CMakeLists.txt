add_library(core STATIC 
    src/syscalls.c
    src/system_stm32wbxx.c
    src/sysmem.c
)
target_include_directories(core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)



target_compile_definitions(core PUBLIC 
    STM32WB55xx 
)