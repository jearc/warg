#include "Scene_Graph.h"
#include "Globals.h"
#include "Render.h"
#include <array>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <atomic>
#include <thread>

glm::mat4 copy(aiMatrix4x4 m)
{
  // assimp is row-major
  // glm is column-major
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
Scene_Graph_Node::Scene_Graph_Node(std::string name, Node_Ptr parent,
                                   const mat4 *basis)
    : parent(parent), name(name)
{
  if (basis)
    import_basis = *basis;
}
Scene_Graph_Node::Scene_Graph_Node(std::string name, const aiNode *node,
                                   const Node_Ptr parent,
                                   const mat4 *import_basis_,
                                   const aiScene *scene,
                                   std::string scene_file_path,
                                   Uint32 *mesh_num,
                                   Material_Descriptor *material_override)
    : parent(parent)
{
  ASSERT(node);
  ASSERT(scene);
  this->name = name;
  basis = copy(node->mTransformation);
  if (import_basis_)
    import_basis = *import_basis_;
  size_t slice = scene_file_path.find_last_of("/\\");
  std::string dir = scene_file_path.substr(0, slice) + '/';

  for (uint32 i = 0; i < node->mNumMeshes; ++i)
  {
    auto ai_i = node->mMeshes[i];
    const aiMesh *aimesh = scene->mMeshes[ai_i];
    (*mesh_num) += 1;
    std::string unique_id = scene_file_path + std::to_string(*mesh_num);
    Mesh mesh(aimesh, unique_id);
    aiMaterial *ptr = scene->mMaterials[aimesh->mMaterialIndex];
    Material material(ptr, dir, material_override);
    model.push_back({mesh, material});
  }
  children.reserve(node->mNumChildren);
}

Scene_Graph::Scene_Graph() : root(0)
{
  Scene_Graph_Node root(std::string("SCENE_GRAPH_ROOT"), Node_Ptr(0), nullptr);

  nodes.reserve(1000);
  nodes.push_back(root);
  nodes.back().children.reserve(1000);
}
void Scene_Graph::add_graph_node(const aiNode *node, Node_Ptr parent,
                                 const mat4 *import_basis,
                                 const aiScene *aiscene,
                                 std::string scene_file_path, Uint32 *mesh_num,
                                 Material_Descriptor *material_override)
{
  ASSERT(node_exists(parent));
  ASSERT(node);
  ASSERT(aiscene);
  std::string name = copy(&node->mName);
  // construct just this node in the main node array
  Scene_Graph_Node node_obj(name, node, parent, import_basis, aiscene,
                            scene_file_path, mesh_num, material_override);
  nodes.push_back(node_obj);

  // give the parent node the pointer to that entity
  const Node_Ptr new_node_ptr = Node_Ptr(nodes.size() - 1);
  ASSERT(node_exists(parent));
  Scene_Graph_Node *this_nodes_parent = get_node(parent);
  this_nodes_parent->children.push_back(new_node_ptr);

  // construct all the new node's children, because its constructor doesn't
  for (uint32 i = 0; i < node->mNumChildren; ++i)
  {
    const aiNode *child = node->mChildren[i];
    add_graph_node(child, new_node_ptr, import_basis, aiscene, scene_file_path,
                   mesh_num, material_override);
  }
}

Node_Ptr Scene_Graph::add_mesh(Mesh_Data m, Material_Descriptor md,
                               std::string name, Node_Ptr parent,
                               const mat4 *import_basis)
{
  Scene_Graph_Node node(name, parent, import_basis);
  Node_Ptr node_ptr(nodes.size());
  Mesh mesh(m,name);
  Material material(md);
  node.model.push_back({mesh, material});
  nodes.push_back(node);
  Scene_Graph_Node *p = get_node(parent);
  p->children.push_back(node_ptr);
  return node_ptr;
}
void Scene_Graph::set_parent(Node_Ptr p, Node_Ptr desired_parent)
{
  Scene_Graph_Node *ptr = this->get_node(p);
  Scene_Graph_Node *dptr = this->get_node(desired_parent);

  Scene_Graph_Node *current_parent = this->get_node(ptr->parent);

  for (auto i = current_parent->children.begin();
       i != current_parent->children.end(); ++i)
  {
    if (i->i == p.i)
    {
      current_parent->children.erase(i);
      break;
    }
  }
  dptr->children.push_back(p);
}

Node_Ptr Scene_Graph::add_aiscene(std::string scene_file_path,
                                  const mat4 *import_basis, Node_Ptr parent,
                                  Material_Descriptor *material_override)
{
  return add_aiscene(load_aiscene(scene_file_path), scene_file_path,
                     import_basis, parent, material_override);
}

Node_Ptr Scene_Graph::add_aiscene(std::string scene_file_path,
                                  Material_Descriptor *material_override)
{
  return add_aiscene(load_aiscene(scene_file_path), scene_file_path, nullptr,
                     Node_Ptr(0), material_override);
}

Node_Ptr Scene_Graph::add_aiscene(const aiScene *scene,
                                  std::string scene_file_path,
                                  const mat4 *import_basis, Node_Ptr parent,
                                  Material_Descriptor *material_override)
{
  // accumulates as meshes are imported, used along with the scene file path
  // to create a unique_id for the mesh
  Uint32 mesh_num = 0;
  const aiNode *root = scene->mRootNode;

  // create the root node for this scene
  std::string name =
      std::string("ROOT FOR: ") + scene_file_path + " " + copy(&root->mName);
  scene_file_path = BASE_MODEL_PATH + scene_file_path;
  Scene_Graph_Node root_for_new_scene(name, root, parent, import_basis, scene,
                                      scene_file_path, &mesh_num,
                                      material_override);

  // push root node to scene graph and parent
  nodes.push_back(root_for_new_scene);
  Node_Ptr root_for_new_scene_ptr = Node_Ptr(nodes.size() - 1);
  Scene_Graph_Node *parent_ptr = get_node(parent);
  parent_ptr->children.push_back(root_for_new_scene_ptr);

  // add every aiscene child to the new node
  const uint32 num_children = scene->mRootNode->mNumChildren;
  nodes.back().children.reserve(num_children);
  for (uint32 i = 0; i < num_children; ++i)
  {
    const aiNode *node = scene->mRootNode->mChildren[i];
    add_graph_node(node, root_for_new_scene_ptr, import_basis, scene,
                   scene_file_path, &mesh_num, material_override);
  }
  return root_for_new_scene_ptr;
}
void Scene_Graph::visit_nodes(const Node_Ptr node_ptr, const mat4 &M,
                              std::vector<Render_Entity> &accumulator)
{
  Scene_Graph_Node *const entity = get_node(node_ptr);
  ASSERT(entity);

  const mat4 T = translate(entity->position);
  const mat4 S = scale(entity->scale);
  const mat4 R = toMat4(entity->orientation);
  const mat4 B = entity->basis;
  const mat4 STACK = M * B * T * S * R;
  const mat4 I = entity->import_basis;
  const mat4 FINAL = STACK * I;

  // TODO optimize: only set lights that will have a noticable effect
  Light_Array affected_lights;
  affected_lights = lights;
  affected_lights.light_count = lights.light_count;

  // Construct render entities from all meshes in this model:
  const uint32 num_meshes = entity->model.size();
  for (uint32 i = 0; i < num_meshes; ++i)
  {
    Mesh *mesh_ptr = &entity->model[i].first;
    Material *material_ptr = &entity->model[i].second;

    // TODO optimize: this is copying all mesh data, because it is kept in the
    // mesh object
    accumulator.emplace_back(mesh_ptr, material_ptr, affected_lights, FINAL);
  }
  const uint32 size = entity->children.size();
  for (uint32 i = 0; i < size; ++i)
  {
    Node_Ptr child = entity->children[i];
    visit_nodes(child, STACK, accumulator);
  }
}
void Scene_Graph::visit_nodes_locked_accumulator(
    Node_Ptr node_ptr, const mat4 &M, std::vector<Render_Entity> *accumulator,
    std::atomic_flag *lock)
{
  Scene_Graph_Node *const entity = get_node(node_ptr);

  const mat4 T = translate(entity->position);
  const mat4 S = scale(entity->scale);
  const mat4 R = toMat4(entity->orientation);
  const mat4 B = entity->basis;
  const mat4 STACK = M * B * T * S * R;
  const mat4 I = entity->import_basis;
  const mat4 FINAL = STACK * I;

  // TODO optimize: only set lights that will have a noticable effect
  Light_Array affected_lights;
  affected_lights = lights;
  affected_lights.light_count = lights.light_count;

  const int num_meshes = entity->model.size();
  for (int i = 0; i < num_meshes; ++i)
  {
    Mesh *mesh_ptr = &entity->model[i].first;
    Material *material_ptr = &entity->model[i].second;
    while (lock->test_and_set())
    { /*spin*/
    }
    accumulator->emplace_back(mesh_ptr, material_ptr, affected_lights, FINAL);
    lock->clear();
  }
  const uint32 size = entity->children.size();
  for (uint32 i = 0; i < size; ++i)
  {
    Node_Ptr child = entity->children[i];
    visit_nodes_locked_accumulator(child, STACK, accumulator, lock);
  }
}

void Scene_Graph::visit_root_node_base_index(
    uint32 node_index, uint32 count, std::vector<Render_Entity> *accumulator,
    std::atomic_flag *lock)
{
  const Scene_Graph_Node *const entity = get_node(Node_Ptr(0));
  ASSERT(entity->name == "SCENE_GRAPH_ROOT");
  ASSERT(entity->position == vec3(0));
  ASSERT(entity->orientation == quat());
  ASSERT(entity->import_basis == mat4(1));

  for (uint32 i = node_index; i < node_index + count; ++i)
  {
    Node_Ptr child = entity->children[i];
    visit_nodes_locked_accumulator(child, mat4(1), accumulator, lock);
  }
}

void Scene_Graph::visit_root_node_base_index_alt(
    uint32 node_index, uint32 count, std::vector<Render_Entity> *accumulator,
    std::atomic_flag *lock)
{
  Scene_Graph_Node *const entity = get_node(Node_Ptr(0));
  ASSERT(entity->name == "SCENE_GRAPH_ROOT");
  ASSERT(entity->position == vec3(0));
  ASSERT(entity->orientation == quat(0, 0, 0, 1));
  ASSERT(entity->import_basis == mat4(1));

  std::vector<Render_Entity> my_accumulator;
  my_accumulator.reserve(count * 1000);
  for (uint32 i = node_index; i < node_index + count; ++i)
  {
    Node_Ptr child = entity->children[i];
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
  const Scene_Graph_Node *rootnode = get_node(Node_Ptr(0));

  const uint32 root_children_count = rootnode->children.size();
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
bool Scene_Graph::node_exists(Node_Ptr ptr) const
{
  ASSERT(ptr.i < nodes.size());
  return ptr.i < nodes.size();
}
uint32 Scene_Graph::node_count() const { return nodes.size(); }

Node_Ptr Scene_Graph::add_primitive_mesh(Mesh_Primitive p, std::string name,
                                         Material_Descriptor m, Node_Ptr parent,
                                         const mat4 *import_basis)
{
  // create the meshes and materials for this node
  Mesh mesh(p, name);
  Material material(m);

  // create the node itself
  Scene_Graph_Node new_node(name, parent, import_basis);
  new_node.model.push_back({mesh, material});

  // push the new node onto this graph
  // construct a ptr to it
  nodes.push_back(new_node);
  Node_Ptr node_ptr = Node_Ptr(nodes.size() - 1);

  // get the parent node pointer
  ASSERT(node_exists(parent));
  Scene_Graph_Node *parent_ptr = get_node(parent);

  // push the new node's pointer onto the parent node
  parent_ptr->children.push_back(node_ptr);

  return node_ptr;
}
