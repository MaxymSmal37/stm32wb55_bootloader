set(TARGET Bootloader)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}//bootloader/STM32WB55CGUX_BOOTLOADER.ld)

add_subdirectory(bootloader_core)
add_subdirectory(bootloader_comm)

add_executable(${TARGET}
    src/main.c
    src/bootloader.c
    ${CMAKE_SOURCE_DIR}/bootloader/src/Startup/startup_stm32wb55cgux.s
)

target_include_directories(${TARGET} PRIVATE
    inc
)

target_link_libraries(${TARGET} PRIVATE 
    bootloader_core
)

target_link_options(${TARGET} PRIVATE
    -T${LINKER_SCRIPT}               
    -Wl,-Map=${TARGET}.map
)
set_target_properties(${TARGET} PROPERTIES
    SUFFIX ".elf"
    LINK_DEPENDS ${LINKER_SCRIPT}    
)

target_link_libraries(${TARGET} PRIVATE 
    bootloader_comm
)