cmake_minimum_required(VERSION 3.10)
project(asLib)
add_library(asLib STATIC audioData.cpp audioData.h autosampler.cpp autosampler.h config.h error.h midiTypes.h wavWriter.cpp wavWriter.h)
target_link_libraries(asLib PUBLIC asBase)
