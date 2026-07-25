add_library(communication STATIC 
    src/communication_protocol.c
)

target_include_directories(communication PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/inc 
)

target_link_libraries(communication PRIVATE core)