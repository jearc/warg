#pragma once
#include "Globals.h"
#include "Render.h"
#include <assimp\Importer.hpp>
#include <assimp\postprocess.h>
#include <assimp\scene.h>
#include <assimp\types.h>
#include <atomic>
#include <glm\glm.hpp>
#include <unordered_map>
struct Material;
struct Material_Descriptor;
struct Scene_Graph;
struct Material_Assigned_Meshes;
struct Assimp_Handle
{
  std::string path;
  const aiScene *scene;
  Assimp::Importer importer;
};
struct Node_Ptr
{
  explicit Node_Ptr(uint32 i) : i(i) {}
  uint32 i;
};
struct Mesh_Ptr
{
  explicit Mesh_Ptr(uint32 i) : i(i) {}
  uint32 i;
};
struct Material_Ptr
{
  explicit Material_Ptr(uint32 i) : i(i) {}
  uint32 i;
};

struct Base_Indices
{
  // uint32 base_node_index;
  int32 index_to_first_mesh = -1;
  int32 index_to_first_material = -1;
};

// does NOT add itself into the owning graph
struct Scene_Graph_Node
{
  std::string name;
  vec3 position = {0, 0, 0};
  vec3 orientation = {0, 0, 0};
  vec3 scale = {1, 1, 1};

private:
  friend struct Scene_Graph;
  Scene_Graph_Node(std::string name, Scene_Graph *owner, Node_Ptr parent,
                   const mat4 *import_basis);
  Scene_Graph_Node(const aiNode *node, const Scene_Graph *owner,
                   Node_Ptr parent, const mat4 *import_basis,
                   const aiScene *scene, Base_Indices base_indices);

  // assimp's import mtransformation, propagates to children
  mat4 basis = mat4(1);

  // user-defined import_basis does NOT propagate down the matrix stack
  // applied before all other transformations
  // essentially, this takes us from model vertex -> our world origin
  // should be the same for every node that was part of the same import
  mat4 import_basis = mat4(1.0f);

  const Scene_Graph *owner;
  const Node_Ptr parent;
  const std::vector<Node_Ptr> *peek_children() const;
  std::vector<Node_Ptr> *get_children();
  void push_child(Node_Ptr ptr);

private:
  // A Material_Assigned_Meshes object contains a bunch of pointers into the
  // scene graph's data pools that make up a list of meshes and their assigned
  // materials
  struct Material_Assigned_Meshes
  {
    Material_Assigned_Meshes();
    Material_Assigned_Meshes(aiNode *node, Scene_Graph *owner,
                             const aiScene *scene);
    uint32 count() const; // number of meshes in this array
    // get() will retrieve a single mesh at index with it's assigned material
    void get(Scene_Graph *graph, uint32 index, Mesh **mesh,
             Material **material) const;
    void push_mesh(Mesh_Ptr mesh_index, Material_Ptr material_index);

  private:
    std::vector<std::pair<Mesh_Ptr, Material_Ptr>> indices;
  };
  Material_Assigned_Meshes meshes;
  std::vector<Node_Ptr> children; // index into scene_graph.nodes
};

struct Scene_Graph
{
  Scene_Graph();

  void add_graph_node(const aiNode *node, Node_Ptr parent,
                      const mat4 *import_basis, const aiScene *aiscene,
                      Base_Indices base_indices);
  const aiScene *load_aiscene(std::string path,
                              Assimp::Importer *importer) const;

  // import_basis intended to only be used as an initial
  // transform from the aiscene's world basis to our world basis
  Node_Ptr add_aiscene(std::string path, const mat4 *import_basis = nullptr,
                       Node_Ptr parent = Node_Ptr(0),
                       std::string vertex_shader = "vertex_shader.vert",
                       std::string fragment_shader = "fragment_shader.frag");

  // returns an index into the scene_graph's entity vector
  // does not yet check for nor cache duplicate meshes/materials
  // Node_Ptr will stay valid for as long as the Scene_Graph is alive
  Node_Ptr add_single_mesh(const char *path, std::string name,
                           Material_Descriptor m, Node_Ptr parent = Node_Ptr(0),
                           const mat4 *basis = nullptr);

  // currently performance bugged
  std::vector<Render_Entity> visit_nodes_async_start();
  std::vector<Render_Entity> visit_nodes_st_start();

  std::array<Light, MAX_LIGHTS> lights;
  uint32 light_count = 0;
  Node_Ptr root;

  // note: these raw pointers are INVALID after any call to any function that
  // adds scene_graph_nodes
  Scene_Graph_Node *get_node(Node_Ptr ptr);
  const Scene_Graph_Node *get_const_node(Node_Ptr ptr) const;
  Mesh *get_mesh(Mesh_Ptr ptr);
  Material *get_material(Material_Ptr ptr);

  bool node_exists(Node_Ptr ptr) const;
  bool mesh_exists(Mesh_Ptr ptr) const;
  bool material_exists(Material_Ptr ptr) const;

  int32 node_count() const; // total node count for the entire scene
private:
  uint32 last_accumulator_size = 0;
  void visit_nodes(const Node_Ptr node_ptr, const mat4 &M,
                   std::vector<Render_Entity> &accumulator);
  void Scene_Graph::visit_nodes_locked_accumulator(
      const Node_Ptr node_ptr, const mat4 &M,
      std::vector<Render_Entity> *accumulator, std::atomic_flag *lock);
  void visit_root_node_base_index(uint32 node_index, uint32 count,
                                  std::vector<Render_Entity> *accumulator,
                                  std::atomic_flag *lock);
  void visit_root_node_base_index_alt(uint32 node_index, uint32 count,
                                      std::vector<Render_Entity> *accumulator,
                                      std::atomic_flag *lock);

  // currently none of these three buffers have a way to delete entries
  // individually
  // ALL meshes/materials/nodes are actually stored contiguously here, not in
  // the tree
  std::vector<Mesh> meshes;
  std::vector<Material> materials;
  std::vector<Scene_Graph_Node> nodes;

  // simple filename ->assimp load result cache
  std::unordered_map<std::string, Assimp_Handle> assimp_cache;

  /*
  This shaded_object_cache will map a (path+vert+frag) string to base indices
  into the mesh/material pools for that particular model
  and material in order for these models to be instanced
  */
  std::unordered_map<std::string, Base_Indices> shaded_object_cache;
};
