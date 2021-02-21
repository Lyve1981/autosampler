cmake_minimum_required(VERSION 3.15)

include("base.cmake")

project(autosampler VERSION 0.0.1)

# ----------------- dependencies

# 3rdparty
add_subdirectory(portaudio)
add_subdirectory(portmidi)

# own
add_subdirectory(asBase)
add_subdirectory(asLib)
add_subdirectory(asCliLib)

# ----------------- command-line app

add_executable(ascli)

target_sources(ascli PRIVATE asCliLib/main.cpp)

target_link_libraries(ascli asCliLib)
