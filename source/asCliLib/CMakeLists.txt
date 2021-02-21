cmake_minimum_required(VERSION 3.10)
project(asCliLib)
add_library(asCliLib STATIC cli.cpp cli.h commandline.cpp commandline.h)
target_link_libraries(asCliLib PUBLIC asLib asBase portaudio_static portmidi-static)
