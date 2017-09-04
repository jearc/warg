cmake_minimum_required (VERSION 2.6)
include (FindPkgConfig)

project (warg)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
set (CMAKE_BUILD_TYPE Debug)
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

file (GLOB CPP_FILES "*.cpp")
file (GLOB HEADER_FILES "*.h" ".hpp")
file (GLOB SHADER_FILES "Assets/Shaders/*.vert" "Assets/Shaders/*.frag")

add_library(warglib ${CPP_FILES} ${HEADER_FILES} ${SHADER_FILES})
add_executable (warg main.cpp)

find_package (Threads REQUIRED)
find_package (SDL2 REQUIRED)
find_package (ASSIMP REQUIRED)
find_package (OpenGL REQUIRED)
find_package (GLEW REQUIRED)
find_package (glm REQUIRED)

include_directories (${GLEW_INCLUDE_DIRS})
include_directories (${OpenGL_INCLUDE_DIRS})
include_directories (${ASSIMP_INCLUDE_DIRS})
include_directories (${glm_INCLUDE_DIRS})
include_directories (${SDL2_INCLUDE_DIRS})

target_link_libraries (warg warglib)
target_link_libraries (warg ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (warg ${GLEW_LIBRARIES})
target_link_libraries (warg ${OPENGL_LIBRARIES})
<<<<<<< HEAD
target_link_libraries (warg ${SDL2_LIBRARIES})
=======
if (WIN32)
target_link_libraries (warg ${SDL2_LIBRARY})
endif (WIN32)
if (UNIX)
target_link_libraries (warg ${SDL2_LIBRARIES})
endif (UNIX)
>>>>>>> a790bb361a92275e6559ee58bdc11e03a536ab44
target_link_libraries (warg ${ASSIMP_LIBRARIES})
