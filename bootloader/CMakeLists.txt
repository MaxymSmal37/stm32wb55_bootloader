set(TARGET Bootloader)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}//bootloader/STM32WB55CGUX_BOOTLOADER.ld)

add_subdirectory(bootloader_core)
add_subdirectory(bootloader_comm)

add_executable(${TARGET}
    src/main.c
    src/bootloader.c
    src/usb_cdc_bootloader.c
    ${CMAKE_SOURCE_DIR}/bootloader/src/Startup/startup_stm32wb55cgux.s
)

target_include_directories(${TARGET} PRIVATE
    inc
    ${CMAKE_SOURCE_DIR}/bootloader/lib/tinyusb/src
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

# Add TinyUSB sources for CDC device support.
set(TINYUSB_SOURCES
    ${CMAKE_SOURCE_DIR}/bootloader/lib/tinyusb/src/tusb.c
    ${CMAKE_SOURCE_DIR}/bootloader/lib/tinyusb/src/common/tusb_fifo.c
    ${CMAKE_SOURCE_DIR}/bootloader/lib/tinyusb/src/device/usbd.c
    ${CMAKE_SOURCE_DIR}/bootloader/lib/tinyusb/src/class/cdc/cdc_device.c
    ${CMAKE_SOURCE_DIR}/bootloader/lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
    ${CMAKE_SOURCE_DIR}/bootloader/lib/tinyusb/src/portable/st/stm32_fsdev/fsdev_common.c
)

target_sources(${TARGET} PRIVATE ${TINYUSB_SOURCES})

target_compile_definitions(${TARGET} PRIVATE
    BOOTLOADER_COMM_USE_USB=1
    CFG_TUSB_MCU=OPT_MCU_STM32WB
    CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE
    CFG_TUSB_OS=OPT_OS_NONE
    CFG_TUD_ENABLED=1
    CFG_TUD_CDC=1
    CFG_TUD_MSC=0
    CFG_TUD_HID=0
    CFG_TUD_MIDI=0
    CFG_TUD_VENDOR=0
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