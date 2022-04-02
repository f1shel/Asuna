cmake_minimum_required(VERSION 3.9.6)
set_property(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT
    asuna)
project(asuna)
add_executable(asuna asuna/main.cpp)