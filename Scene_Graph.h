#pragma once
#include "Globals.h"
#include "Render.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <atomic>
#include <glm/glm.hpp>
#include <unordered_map>
struct Material;
struct Material_Descriptor;
struct Scene_Graph;
struct Material_Assigned_Meshes;

struct Node_Ptr
{
  explicit Node_Ptr(uint32 i) : i(i) {}
  Node_Ptr(){};
  uint32 i = -1;
};

struct Scene_Graph_Node
{
  uint32 ID = 0;
  std::string name;
  vec3 position = {0, 0, 0};
  quat orientation;
  vec3 scale = {1, 1, 1};
  vec3 velocity = {0, 0, 0};

  std::vector<std::pair<Mesh, Material>> model;

private:
  friend struct Scene_Graph;
  Scene_Graph_Node(std::string name, Node_Ptr parent, const mat4 *import_basis);
  Scene_Graph_Node(std::string name, const aiNode *node, Node_Ptr parent,
                   const mat4 *import_basis_, const aiScene *scene,
                   std::string scene_path, Uint32 *mesh_num,
                   Material_Descriptor *material_override);

  // assimp's import mtransformation, propagates to children
  mat4 basis = mat4(1);

  // user-defined import_basis does NOT propagate down the matrix stack
  // applied before all other transformations
  // essentially, this takes us from import basis -> our world basis
  // should be the same for every node that was part of the same import
  mat4 import_basis = mat4(1);

  const Node_Ptr parent;

  std::vector<Node_Ptr> children; // index into scene_graph.nodes
};

struct Scene_Graph
{
  Scene_Graph();

  void set_parent(Node_Ptr p, Node_Ptr desired_parent);

  Node_Ptr add_aiscene(std::string scene_file_path,
                       Material_Descriptor *material_override = nullptr);

  Node_Ptr add_aiscene(std::string scene_file_path, const mat4 *import_basis,
                       Node_Ptr parent = Node_Ptr(0),
                       Material_Descriptor *material_override = nullptr);

  Node_Ptr add_aiscene(const aiScene *scene, std::string asset_path,
                       const mat4 *import_basis = nullptr,
                       Node_Ptr parent = Node_Ptr(0),
                       Material_Descriptor *material_override = nullptr);

  // construct a node using the load_mesh function in Mesh_Loader
  // does not yet check for nor cache duplicate meshes/materials
  // Node_Ptr will stay valid for as long as the Scene_Graph is alive
  Node_Ptr add_primitive_mesh(Mesh_Primitive p, std::string name,
                              Material_Descriptor m,
                              Node_Ptr parent = Node_Ptr(0),
                              const mat4 *import_basis = nullptr);

  Node_Ptr add_mesh(Mesh_Data m, Material_Descriptor md, std::string name,
                    Node_Ptr parent = Node_Ptr(0),
                    const mat4 *import_basis = nullptr);

  // traverse the entire graph, computing the final transformation matrices
  // for each entity, and return all entities flatted into a vector
  // async currently performance bugged on w7
  std::vector<Render_Entity> visit_nodes_async_start();

  // traverse the entire graph, computing the final transformation matrices
  // for each entity, and return all entities flatted into a vector
  std::vector<Render_Entity> visit_nodes_st_start();

  // renderer assumes all active lights are lights [0,light_count)
  Light_Array lights;

  // root node for entire scene graph
  Node_Ptr root;

  // get graph node pointer from Node_Ptr
  // note: these raw pointers are INVALID after any call to any function that
  // adds scene_graph_nodes
  Scene_Graph_Node *get_node(Node_Ptr ptr);

  // returns true if a Node_Ptr exists
  bool node_exists(Node_Ptr ptr) const;

  // returns the total node count for the entire scene
  uint32 node_count() const;

private:
  // add a Scene_Graph_Node to the Scene_Graph using an aiNode, aiScene, and
  // parent Node_Ptr
  void add_graph_node(const aiNode *node, Node_Ptr parent,
                      const mat4 *import_basis, const aiScene *aiscene,
                      std::string path, Uint32 *mesh_num,
                      Material_Descriptor *material_override);

  uint32 last_accumulator_size = 0;

  // various node traversal algorithms
  void visit_nodes(Node_Ptr node_ptr, const mat4 &M,
                   std::vector<Render_Entity> &accumulator);
  void visit_nodes_locked_accumulator(Node_Ptr node_ptr, const mat4 &M,
                                      std::vector<Render_Entity> *accumulator,
                                      std::atomic_flag *lock);
  void visit_root_node_base_index(uint32 node_index, uint32 count,
                                  std::vector<Render_Entity> *accumulator,
                                  std::atomic_flag *lock);
  void visit_root_node_base_index_alt(uint32 node_index, uint32 count,
                                      std::vector<Render_Entity> *accumulator,
                                      std::atomic_flag *lock);

  std::vector<Scene_Graph_Node> nodes;
};
