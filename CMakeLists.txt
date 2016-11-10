cmake_minimum_required (VERSION 2.6)

include (FindPkgConfig)

project (warg)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")

pkg_search_module (SDL2 REQUIRED sdl2)
find_package (OpenGL REQUIRED)
find_package (GLEW REQUIRED)

add_executable (warg "main.cpp")

include_directories (${GLEW_INCLUDE_DIRS} ${OpenGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
target_link_libraries (warg ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES})
