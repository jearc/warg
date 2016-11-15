#include "Scene_Graph.h"
#include "Render.h"
#include <array>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <atomic>
#include <thread>

// TODO optimize: hashtable the path of each loaded mesh as the key, include
// indices into the mesh/material vectors as the value, construct new meshes if
// the value doesnt exist, skip construction and use the indices if they do
// exist, and if you do the next optimization, destroy the key/value when you
// free the mesh/material
// TODO optimize: make these private, make a getter function that will count the
// times each index is accessed per frame, if the render prepare function
// completes without touching one of them, it has no references, and should be
// safe to free
// to free, you can mark the index as 'free', unload it (but dont delete the
// mesh object from the pool vector - that invalidates the other indices as they
// are shared), and re-use the index for the next mesh that is loaded

// if multithreading make a get_const_ptr and get_ptr with different locks,
// get_const_ptr needs to lock just for reallocation of vector its inside of,
// get_ptr needs to lock that entire entity

glm::mat4 copy(aiMatrix4x4 m)
{
  // assimp is row-major
  // glm is column-major
  const mat4 I(1);
  glm::mat4 result;
  for (uint32 i = 0; i < 4; ++i)
  {
    for (uint32 j = 0; j < 4; ++j)
    {
      result[i][j] = m[j][i];
    }
  }
  return result;
}

Scene_Graph_Node::Material_Assigned_Meshes::Material_Assigned_Meshes() {}
Scene_Graph_Node::Material_Assigned_Meshes::Material_Assigned_Meshes(
    aiNode *node, Scene_Graph *owner, const aiScene *scene)
{
}

// number of meshes in this array
uint32 Scene_Graph_Node::Material_Assigned_Meshes::count() const
{
  return indices.size();
}
void Scene_Graph_Node::Material_Assigned_Meshes::get(Scene_Graph *graph,
                                                     uint32 index, Mesh **mesh,
                                                     Material **material) const
{
  ASSERT(index < indices.size());
  ASSERT(index >= 0);
  ASSERT(mesh);
  ASSERT(material);
  const std::pair<Mesh_Ptr, Material_Ptr> model = indices[index];
  *mesh = graph->get_mesh(model.first);
  *material = graph->get_material(model.second);
}
void Scene_Graph_Node::Material_Assigned_Meshes::push_mesh(
    Mesh_Ptr mesh_index, Material_Ptr material_index)
{
  indices.push_back({mesh_index, material_index});
}
Scene_Graph_Node::Scene_Graph_Node(std::string name, Scene_Graph *owner,
                                   Node_Ptr parent, const mat4 *import_basis)
    : owner(owner), parent(parent), name(name)
{
  if (import_basis)
    this->import_basis = *import_basis;
}
Scene_Graph_Node::Scene_Graph_Node(const aiNode *node, const Scene_Graph *owner,
                                   Node_Ptr parent, const mat4 *import_basis_,
                                   const aiScene *scene,
                                   Base_Indices base_indices)
    : owner(owner), parent(parent)
{
  ASSERT(node);
  ASSERT(scene);
  ASSERT(owner);

  name = copy(node->mName);

  basis = copy(node->mTransformation);

  if (import_basis_)
    import_basis = *import_basis_;

  for (uint32 i = 0; i < node->mNumMeshes; ++i)
  {
    // create the pointer to where this mesh should be in our mesh pool
    uint32 ai_index = node->mMeshes[i];
    const Mesh_Ptr mesh_ptr =
        Mesh_Ptr(ai_index + base_indices.index_to_first_mesh);
    const aiMesh *mesh = scene->mMeshes[ai_index];
    ASSERT(mesh);
    const Material_Ptr material_ptr = Material_Ptr(
        mesh->mMaterialIndex + base_indices.index_to_first_material);

    ASSERT(owner->mesh_exists(mesh_ptr));
    ASSERT(owner->material_exists(material_ptr));

    meshes.push_mesh(mesh_ptr, material_ptr);
  }
  children.reserve(node->mNumChildren);
}
const std::vector<Node_Ptr> *Scene_Graph_Node::peek_children() const
{
  return &children;
}
std::vector<Node_Ptr> *Scene_Graph_Node::get_children() { return &children; }
void Scene_Graph_Node::push_child(Node_Ptr ptr) { children.push_back(ptr); }

Scene_Graph::Scene_Graph() : root(0)
{
  Scene_Graph_Node root(std::string("SCENE_GRAPH_ROOT"), this, Node_Ptr(0),
                        nullptr);
  nodes.reserve(1000);
  meshes.reserve(1000);
  materials.reserve(1000);
  nodes.push_back(root);
  nodes.back().get_children()->reserve(1000);
}
void Scene_Graph::add_graph_node(const aiNode *node, Node_Ptr parent,
                                 const mat4 *import_basis,
                                 const aiScene *aiscene,
                                 Base_Indices base_indices)
{
  ASSERT(node_exists(parent));
  // construct just this node in the main node array
  Scene_Graph_Node node_obj(node, this, parent, import_basis, aiscene,
                            base_indices);
  nodes.push_back(node_obj);

  // give the parent node the pointer to that entity
  const Node_Ptr new_node_ptr = Node_Ptr(nodes.size() - 1);
  Scene_Graph_Node *this_nodes_parent = get_node(parent);
  this_nodes_parent->push_child(new_node_ptr);

  // construct all the new node's children, because its constructor doesn't
  for (uint32 i = 0; i < node->mNumChildren; ++i)
  {
    const aiNode *child = node->mChildren[i];
    add_graph_node(child, new_node_ptr, import_basis, aiscene, base_indices);
  }
}
const aiScene *Scene_Graph::load_aiscene(std::string path,
                                         Assimp::Importer *importer) const
{
  ASSERT(importer);
  auto flags = aiProcess_FlipWindingOrder | aiProcess_Triangulate |
               aiProcess_FlipUVs | aiProcess_CalcTangentSpace |
               // aiProcess_MakeLeftHanded|
               aiProcess_PreTransformVertices | aiProcess_GenUVCoords |
               // aiProcess_OptimizeGraph|
               // aiProcess_ImproveCacheLocality|
               //  aiProcess_OptimizeMeshes|
               0;

  std::string final_path = BASE_MODEL_PATH + path;
  const aiScene *aiscene = importer->ReadFile(final_path.c_str(), flags);
  if (!aiscene || aiscene->mFlags == AI_SCENE_FLAGS_INCOMPLETE ||
      !aiscene->mRootNode)
  {
    std::cout << "ERROR::ASSIMP::" << importer->GetErrorString() << std::endl;
    ASSERT(0);
  }
  return aiscene;
}

// import_basis intended to only be used as an initial
// transform from the aiscene's world basis to our world basis
Node_Ptr Scene_Graph::add_aiscene(std::string path, const mat4 *import_basis,
                                  Node_Ptr parent, std::string vertex_shader,
                                  std::string fragment_shader)
{
  const aiScene *scene;
  auto cache_entry = &assimp_cache[path];
  if (!cache_entry->scene)
  {
    cache_entry->path = path;
    cache_entry->scene = load_aiscene(path, &cache_entry->importer);
  }
  scene = cache_entry->scene;
  // a duplicate scene added needs to be able to point to the first scene's
  // mesh and material(if shaders are the same) entries in the pool
  // in order for them to be recognized and packed for instancing

  // map string(path+vert+frag) -> base_indices and skip mesh/mat copy
  Base_Indices base_indices;
  base_indices.index_to_first_mesh = meshes.size();
  base_indices.index_to_first_material = materials.size();
  std::string key_value = path + vertex_shader + fragment_shader;
  Base_Indices *entry = &shaded_object_cache[key_value];
  if (entry->index_to_first_mesh != -1)
  { // entry was already present, use these indices
    base_indices = *entry;
  }
  else
  { // entry missing, add mesh data and assign base indices
    *entry = base_indices;
    for (uint32 i = 0; i < scene->mNumMeshes; ++i)
    {
      meshes.emplace_back(scene->mMeshes[i]);
    }
    for (uint32 i = 0; i < scene->mNumMaterials; ++i)
    {
      aiMaterial *ptr = scene->mMaterials[i];
      size_t slice = path.find_last_of("/\\");
      std::string dir = path.substr(0, slice);
      dir += "/";
      Material m(ptr, dir, vertex_shader, fragment_shader);
      materials.push_back(m);
    }
  }
  // create a node that will hold all the root node's children
  Scene_Graph_Node root_for_new_scene(std::string("ROOT FOR: ") + path, this,
                                      parent, import_basis);

  // push root node to scene graph and parent
  // root for new_scene_ptr will contain all of this aiscene's children
  nodes.push_back(root_for_new_scene);
  Node_Ptr root_for_new_scene_ptr = Node_Ptr(nodes.size() - 1);
  Scene_Graph_Node *parent_ptr = get_node(parent);
  parent_ptr->push_child(root_for_new_scene_ptr);

  // add every aiscene child to the new node
  const uint32 num_children = scene->mRootNode->mNumChildren;
  parent_ptr->get_children()->reserve(num_children);
  for (uint32 i = 0; i < num_children; ++i)
  {
    const aiNode *node = scene->mRootNode->mChildren[i];
    add_graph_node(node, root_for_new_scene_ptr, import_basis, scene,
                   base_indices);
  }
  return root_for_new_scene_ptr;
}
void Scene_Graph::visit_nodes(const Node_Ptr node_ptr, const mat4 &M,
                              std::vector<Render_Entity> &accumulator)
{
  const Scene_Graph_Node *const entity = get_const_node(node_ptr);
  ASSERT(entity);

  const vec3 o = entity->orientation;
  const mat4 T = translate(entity->position);
  const mat4 S = scale(entity->scale);
  const mat4 R = eulerAngleYXZ(o.y, o.x, o.z);
  const mat4 B = entity->basis;
  const mat4 STACK = M * B * T * S * R;
  const mat4 I = entity->import_basis;
  const mat4 FINAL = STACK * I;

  // TODO optimize: only set lights that will have a noticable effect
  Light_Array affected_lights;
  affected_lights.lights = lights;
  affected_lights.count = light_count;

  // Construct render entities from all meshes in this model:
  const uint32 num_meshes = entity->meshes.count();
  for (uint32 i = 0; i < num_meshes; ++i)
  {
    Mesh *mesh_ptr;
    Material *material_ptr;
    entity->meshes.get(this, i, &mesh_ptr, &material_ptr);

    // TODO optimize: this is copying all mesh data, because it is kept in the
    // mesh object
    accumulator.emplace_back(mesh_ptr, material_ptr, affected_lights, FINAL);
  }
  const std::vector<Node_Ptr> *children = entity->peek_children();
  for (uint32 i = 0; i < children->size(); ++i)
  {
    Node_Ptr child = (*entity->peek_children())[i];
    visit_nodes(child, STACK, accumulator);
  }
}
void Scene_Graph::visit_nodes_locked_accumulator(
    const Node_Ptr node_ptr, const mat4 &M,
    std::vector<Render_Entity> *accumulator, std::atomic_flag *lock)
{
  const Scene_Graph_Node *const entity = get_const_node(node_ptr);

  const vec3 o = entity->orientation;
  const mat4 T = translate(entity->position);
  const mat4 S = scale(entity->scale);
  const mat4 R = eulerAngleYXZ(o.y, o.x, o.z);
  const mat4 B = entity->basis;
  const mat4 STACK = M * B * T * S * R;
  const mat4 I = entity->import_basis;
  const mat4 FINAL = STACK * I;

  Light_Array affected_lights;
  affected_lights.lights = lights;
  affected_lights.count = light_count;

  const int num_meshes = entity->meshes.count();
  for (int i = 0; i < num_meshes; ++i)
  {
    Mesh *mesh_ptr;
    Material *material_ptr;
    entity->meshes.get(this, i, &mesh_ptr, &material_ptr);

    while (lock->test_and_set())
    { /*spin*/
    }
    accumulator->emplace_back(mesh_ptr, material_ptr, affected_lights, FINAL);
    lock->clear();
  }
  const std::vector<Node_Ptr> *children = entity->peek_children();
  const uint32 size = children->size();
  for (uint32 i = 0; i < size; ++i)
  {
    Node_Ptr child = (*entity->peek_children())[i];
    visit_nodes_locked_accumulator(child, STACK, accumulator, lock);
  }
}

void Scene_Graph::visit_root_node_base_index(
    uint32 node_index, uint32 count, std::vector<Render_Entity> *accumulator,
    std::atomic_flag *lock)
{
  const Scene_Graph_Node *const entity = get_const_node(Node_Ptr(0));
  ASSERT(entity->name == "SCENE_GRAPH_ROOT");
  ASSERT(entity->position == vec3(0));
  ASSERT(entity->orientation == vec3(0));
  ASSERT(entity->import_basis == mat4(1));

  const std::vector<Node_Ptr> *children = entity->peek_children();
  const uint32 size = children->size();
  for (uint32 i = node_index; i < node_index + count; ++i)
  {
    const std::vector<Node_Ptr> *v = entity->peek_children();
    Node_Ptr child = (*v)[i];
    visit_nodes_locked_accumulator(child, mat4(1), accumulator, lock);
  }
}

void Scene_Graph::visit_root_node_base_index_alt(
    uint32 node_index, uint32 count, std::vector<Render_Entity> *accumulator,
    std::atomic_flag *lock)
{
  const Scene_Graph_Node *const entity = get_const_node(Node_Ptr(0));
  ASSERT(entity->name == "SCENE_GRAPH_ROOT");
  ASSERT(entity->position == vec3(0));
  ASSERT(entity->orientation == vec3(0));
  ASSERT(entity->import_basis == mat4(1));

  const std::vector<Node_Ptr> *children = entity->peek_children();
  const uint32 size = children->size();
  std::vector<Render_Entity> my_accumulator;
  my_accumulator.reserve(count * 1000);
  for (uint32 i = node_index; i < node_index + count; ++i)
  {
    const std::vector<Node_Ptr> *v = entity->peek_children();
    Node_Ptr child = (*v).at(i);
    visit_nodes(child, mat4(1), my_accumulator);
  }
  while (lock->test_and_set())
  {
  }
  std::copy(my_accumulator.begin(), my_accumulator.end(),
            back_inserter(*accumulator));
  lock->clear();
}
std::vector<Render_Entity> Scene_Graph::visit_nodes_async_start()
{
  std::vector<Render_Entity> accumulator;
  accumulator.reserve(uint32(last_accumulator_size * 1.5));
  const Scene_Graph_Node *rootnode = get_const_node(Node_Ptr(0));
  const auto children = rootnode->peek_children();

  const uint32 root_children_count = children->size();
  const uint32 thread_count = 4;
  const uint32 nodes_per_thread = root_children_count / thread_count;
  const uint32 leftover_nodes = root_children_count % thread_count;

  PERF_TIMER.start();
  // todo the refactoring into class member function(or regression to W7 from
  // W10)
  // or other change killed the performance here
  std::thread threads[thread_count];
  std::atomic_flag lock = ATOMIC_FLAG_INIT;
  for (uint32 i = 0; i < thread_count; ++i)
  {
    const uint32 base_index = i * nodes_per_thread;
    auto fp = [this, base_index, nodes_per_thread, &accumulator, &lock] {
      this->visit_root_node_base_index(base_index, nodes_per_thread,
                                       &accumulator, &lock);
    };
    threads[i] = std::thread(fp);
  }
  PERF_TIMER.stop();
  uint32 leftover_index = (thread_count * nodes_per_thread);
  ASSERT(leftover_index + leftover_nodes == root_children_count);
  visit_root_node_base_index(leftover_index, leftover_nodes, &accumulator,
                             &lock);
  for (int i = 0; i < thread_count; ++i)
  {
    threads[i].join();
  }

  last_accumulator_size = accumulator.size();
  return accumulator;
}
std::vector<Render_Entity> Scene_Graph::visit_nodes_st_start()
{
  std::vector<Render_Entity> accumulator;
  accumulator.reserve(uint32(last_accumulator_size * 1.5));
  visit_nodes(Node_Ptr(0), mat4(1), accumulator);
  last_accumulator_size = accumulator.size();
  return accumulator;
}

Scene_Graph_Node *Scene_Graph::get_node(Node_Ptr ptr)
{
  ASSERT(node_exists(ptr));
  return &nodes[ptr.i];
}
const Scene_Graph_Node *Scene_Graph::get_const_node(Node_Ptr ptr) const
{
  ASSERT(node_exists(ptr));
  return &nodes[ptr.i];
}
// note: these raw pointers are INVALID after any call to any function
// that adds scene_graph_nodes
Mesh *Scene_Graph::get_mesh(Mesh_Ptr ptr)
{
  ASSERT(mesh_exists(ptr));
  return &meshes[ptr.i];
}
// note: these raw pointers are INVALID after any call to any function
// that adds scene_graph_nodes
Material *Scene_Graph::get_material(Material_Ptr ptr)
{
  ASSERT(material_exists(ptr));
  return &materials[ptr.i];
}
bool Scene_Graph::node_exists(Node_Ptr ptr) const
{
  ASSERT(ptr.i < nodes.size());
  ASSERT(nodes[ptr.i].owner == this);
  return ptr.i < nodes.size();
}
bool Scene_Graph::mesh_exists(Mesh_Ptr ptr) const
{
  ASSERT(ptr.i < meshes.size());
  ASSERT(ptr.i < INT32_MAX);
  return ptr.i < meshes.size();
}
bool Scene_Graph::material_exists(Material_Ptr ptr) const
{
  ASSERT(ptr.i < materials.size());
  ASSERT(ptr.i < INT32_MAX);
  return ptr.i < materials.size();
}
int32 Scene_Graph::node_count() const { return nodes.size(); }

Node_Ptr Scene_Graph::add_single_mesh(const char *path, std::string name,
                                      Material_Descriptor m, Node_Ptr parent,
                                      const mat4 *import_basis)
{
  // load the meshes and materials for this node
  meshes.emplace_back(load_mesh(path), name);
  materials.emplace_back(m);
  Mesh_Ptr mesh_index = Mesh_Ptr(meshes.size() - 1);
  Material_Ptr material_index = Material_Ptr(materials.size() - 1);

  // create the node itself
  Scene_Graph_Node new_node(name, this, parent, import_basis);
  new_node.meshes.push_mesh(mesh_index, material_index);

  // push the new node onto this graph
  // construct a ptr to it
  nodes.push_back(new_node);
  Node_Ptr node_ptr = Node_Ptr(nodes.size() - 1);

  // get the parent node pointer
  ASSERT(node_exists(parent));
  Scene_Graph_Node *parent_ptr = get_node(parent);

  // push the new node's pointer onto the parent node
  parent_ptr->push_child(node_ptr);

  return node_ptr;
}
