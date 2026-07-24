add_library(bootloader STATIC 
    src/bootloader.c
)

target_include_directories(bootloader PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/inc 
)

target_link_libraries(bootloader PRIVATE core)