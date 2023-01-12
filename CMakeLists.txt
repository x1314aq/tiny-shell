cmake_minimum_required(VERSION 3.8)
project(tiny-shell
        DESCRIPTION "Tiny shell running on Windows"
        LANGUAGES C CXX
        VERSION 1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_FILES
    builtin.cpp
    tiny-shell.cpp
    win_getopt.c)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Werror>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Werror -fno-exceptions>)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _CONSOLE _UNICODE UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:_NDEBUG>)

target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-municode -mconsole>)

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    CXX_STANDARD 14)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_BINARY_DIR})