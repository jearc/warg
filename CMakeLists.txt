cmake_minimum_required (VERSION 2.6)

project (warg)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")

set (CMAKE_BUILD_TYPE Debug)

file (GLOB CPP_FILES "*.cpp")
file (GLOB HEADER_FILES "*.h" ".hpp")
file (GLOB SHADER_FILES "Assets/Shaders/*.vert" "Assets/Shaders/*.frag")

add_executable (warg ${CPP_FILES} ${HEADER_FILES} ${SHADER_FILES})

include (FindPkgConfig)

pkg_search_module (SDL2 REQUIRED sdl2)
find_package (ASSIMP REQUIRED)
find_package (OpenGL REQUIRED)
find_package (GLEW REQUIRED)

include_directories (${GLEW_INCLUDE_DIRS} ${OpenGL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS} )
target_link_libraries (warg pthread ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES} ${ASSIMP_LIBRARIES} )
