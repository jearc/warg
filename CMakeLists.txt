cmake_minimum_required (VERSION 2.6)
project (warg)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")

find_package(PkgConfig REQUIRED)

find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

find_package(OpenGL REQUIRED)
include_directories(${OpenGL_INCLUDE_DIRS})

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable (warg "main.cpp")

target_link_libraries(warg ${GLEW_LIBRARIES})
target_link_libraries(warg -lGL ${OpenGL_LIBRARIES})
target_link_libraries(warg ${SDL2_LIBRARIES})
