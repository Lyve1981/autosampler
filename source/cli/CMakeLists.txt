cmake_minimum_required(VERSION 3.10)
project(cli)
add_library(cli STATIC cli.cpp cli.h commandline.cpp commandline.h)
target_link_libraries(cli PUBLIC lib base portaudio_static portmidi-static)
