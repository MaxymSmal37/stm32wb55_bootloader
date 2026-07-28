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

# set_property(CACHE BOOTLOADER_COMM_TRANSPORT PROPERTY STRINGS UART USB)

# if(BOOTLOADER_COMM_TRANSPORT STREQUAL "USB")
#     add_compile_definitions(BOOTLOADER_COMM_USE_USB=1)
# elseif(BOOTLOADER_COMM_TRANSPORT STREQUAL "UART")
#     add_compile_definitions(BOOTLOADER_COMM_USE_UART=1)
# else()
#     message(FATAL_ERROR "Unsupported BOOTLOADER_COMM_TRANSPORT '${BOOTLOADER_COMM_TRANSPORT}'. Use UART or USB.")
# endif()


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