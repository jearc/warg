cmake_minimum_required (VERSION 2.6)

project (warg)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")

set (CMAKE_BUILD_TYPE Debug)

add_executable (warg "main.cpp" "State.cpp" "Shader.cpp" "Scene_Graph.cpp" "Render.cpp" "Mesh_Loader.cpp" "Globals.cpp")

include (FindPkgConfig)

pkg_search_module (SDL2 REQUIRED sdl2)
find_package (ASSIMP REQUIRED)
find_package (OpenGL REQUIRED)
find_package (GLEW REQUIRED)

include_directories (${GLEW_INCLUDE_DIRS} ${OpenGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS} )
target_link_libraries (warg pthread ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES} ${ASSIMP_LIBRARIES} )
