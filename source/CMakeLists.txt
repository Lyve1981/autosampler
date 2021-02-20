cmake_minimum_required(VERSION 3.15)

include("base.cmake")

project(autosampler VERSION 0.0.1)

# ----------------- dependencies

# 3rdparty
add_subdirectory(portaudio)
add_subdirectory(portmidi)

# own
add_subdirectory(base)
add_subdirectory(lib)
add_subdirectory(cli)

# ----------------- command-line app

add_executable(ascli)

target_sources(ascli PRIVATE cli/main.cpp)

target_link_libraries(ascli cli)
