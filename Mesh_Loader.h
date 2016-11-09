#pragma once
#include "Globals.h"
#include <assimp\Importer.hpp>
#include <assimp\postprocess.h>
#include <assimp\scene.h>
#include <assimp\types.h>
#include <glm\glm.hpp>
#include <vector>
struct Mesh_Data
{
  std::vector<vec3> positions;
  std::vector<vec3> normals;
  std::vector<vec2> texture_coordinates;
  std::vector<vec3> tangents;
  std::vector<vec3> bitangents;
  std::vector<uint32> indices;
  std::string name;
};

// expects clockwise abcd vertices for front-facing side
void add_quad(vec3 a, vec3 b, vec3 c, vec3 d, Mesh_Data &mesh);
Mesh_Data load_mesh_cube();
Mesh_Data load_mesh_plane();
Mesh_Data load_mesh(const char *path);
void copy_mesh_data(std::vector<vec3> &dst, aiVector3D *src, uint32 length);
void copy_mesh_data(std::vector<vec2> &dst, aiVector3D *src, uint32 length);
Mesh_Data load_mesh(const aiMesh *aimesh);