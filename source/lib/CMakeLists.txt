cmake_minimum_required(VERSION 3.10)
project(lib)
add_library(lib STATIC audioData.cpp audioData.h autosampler.cpp autosampler.h config.h error.h midiTypes.h wavWriter.cpp wavWriter.h)
target_link_libraries(lib PUBLIC base)
