cmake_minimum_required(VERSION 3.14)

project(png_text_chunk)
add_executable(png_text_chunk main.cpp CRC.h png_text_chunk.hpp)

target_compile_options(png_text_chunk PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /source-charset:utf-8 /Zc:__cplusplus /Zc:preprocessor>
)
target_compile_features(png_text_chunk PRIVATE cxx_std_17)

set(resources ${CMAKE_CURRENT_LIST_DIR}/orbit.png)
add_custom_command(TARGET png_text_chunk POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${resources} $<TARGET_FILE_DIR:png_text_chunk>)
