cmake_minimum_required (VERSION 2.6)
project (warg)

add_executable (warg "main.cpp" "State.cpp" "Shader.cpp" "Scene_Graph.cpp" "Render.cpp" "Mesh_Loader.cpp" "Globals.cpp" "State.h" "Shader.h" "Scene_Graph.h" "Render.h" "Mesh_Loader.h" "Globals.h")


find_package (ASSIMP REQUIRED)
find_package (SDL REQUIRED)
find_package (OpenGL REQUIRED)
find_package (GLEW REQUIRED)

include_directories (${GLEW_INCLUDE_DIRS} ${OpenGL_INCLUDE_DIRS} ${SDL_INCLUDE_DIR} ${ASSIMP_INCLUDE_DIRS} )
target_link_libraries (warg ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL_LIBRARY} $(SDLMAIN_LIBRARY) ${ASSIMP_LIBRARY_RELEASE} )