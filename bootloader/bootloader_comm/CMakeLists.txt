add_library(bootloader_comm STATIC 
    src/communication_protocol.c
)

target_include_directories(bootloader_comm PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/bootloader/inc
)

target_link_libraries(bootloader_comm PRIVATE bootloader_core)