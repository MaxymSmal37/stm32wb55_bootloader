set(TARGET STM32wb55_App)

# Absolute path to the FLASH linker script kept in the repo root.
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32WB55CGUX_FLASH.ld)

add_executable(${TARGET}
    src/main.c
    Startup/startup_stm32wb55cgux.s
)

target_include_directories(${TARGET} PRIVATE
    inc
)

target_link_libraries(${TARGET} PRIVATE 
    communication
    core
)


# Hand the linker script to the linker and re-link if it ever changes.
target_link_options(${TARGET} PRIVATE
    -T${LINKER_SCRIPT}
    -Wl,-Map=${TARGET}.map
)
set_target_properties(${TARGET} PROPERTIES
    SUFFIX ".elf"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

# ----------------------------------------------------------------------------
# Post-build: print size, and emit .hex / .bin for flashing.
# ----------------------------------------------------------------------------
add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET}> ${TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
    COMMENT "Generating ${TARGET}.hex and ${TARGET}.bin"
)
