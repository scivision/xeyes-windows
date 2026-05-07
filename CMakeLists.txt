cmake_minimum_required(VERSION 3.19)

project(WinEyes LANGUAGES CXX)

add_executable(wineyes WIN32 wineyes.cpp WINEYES.RC)
target_include_directories(wineyes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
