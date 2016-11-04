cmake_minimum_required (VERSION 2.6)
project (warg)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")

add_executable (warg "main.cpp")

target_link_libraries(warg -lGLEW -lGL -lSDL2)