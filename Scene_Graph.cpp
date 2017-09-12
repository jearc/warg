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

using namespace std;

uint32 new_ID()
{
  static uint32 last = 0;
  return last += 1;
}

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

Scene_Graph_Node::Scene_Graph_Node(string name, const mat4 *basis)
{
  this->name = name;
  if (basis)
    this->import_basis = *basis;
}

Scene_Graph_Node::Scene_Graph_Node(string name, const aiNode *node,
                                   const mat4 *import_basis_,
                                   const aiScene *scene, string scene_file_path,
                                   Uint32 *mesh_num,
                                   Material_Descriptor *material_override)
{
  ASSERT(node);
  ASSERT(scene);
  this->name = name;
  basis = copy(node->mTransformation);
  if (import_basis_)
    import_basis = *import_basis_;
  size_t slice = scene_file_path.find_last_of("/\\");
  string dir = scene_file_path.substr(0, slice) + '/';

  for (uint32 i = 0; i < node->mNumMeshes; ++i)
  {
    auto ai_i = node->mMeshes[i];
    const aiMesh *aimesh = scene->mMeshes[ai_i];
    (*mesh_num) += 1;
    string unique_id = scene_file_path + to_string(*mesh_num);
    Mesh mesh(aimesh, unique_id);
    aiMaterial *ptr = scene->mMaterials[aimesh->mMaterialIndex];
    Material material(ptr, dir, material_override);
    model.push_back({mesh, material});
  }
  children.reserve(node->mNumChildren);
}

Scene_Graph::Scene_Graph()
{
  root = make_shared<Scene_Graph_Node>("SCENE_GRAPH_ROOT", nullptr);
}
void Scene_Graph::add_graph_node(const aiNode *node,
                                 weak_ptr<Scene_Graph_Node> parent,
                                 const mat4 *import_basis,
                                 const aiScene *aiscene, string scene_file_path,
                                 Uint32 *mesh_num,
                                 Material_Descriptor *material_override)
{
  ASSERT(node);
  ASSERT(aiscene);
  string name = copy(&node->mName);
  // construct just this node in the main node array
  auto thing = Scene_Graph_Node(name, node, import_basis, aiscene,
                                scene_file_path, mesh_num, material_override);

  shared_ptr<Scene_Graph_Node> new_node = make_shared<Scene_Graph_Node>(
      name, node, import_basis, aiscene, scene_file_path, mesh_num,
      material_override);

  set_parent(new_node, parent);

  // construct all the new node's children, because its constructor doesn't
  for (uint32 i = 0; i < node->mNumChildren; ++i)
  {
    const aiNode *child = node->mChildren[i];
    add_graph_node(child, new_node, import_basis, aiscene, scene_file_path,
                   mesh_num, material_override);
  }
}

shared_ptr<Scene_Graph_Node>
Scene_Graph::add_mesh(Mesh_Data m, Material_Descriptor md, string name,
                      weak_ptr<Scene_Graph_Node> parent,
                      const mat4 *import_basis)
{
  shared_ptr<Scene_Graph_Node> node =
      make_shared<Scene_Graph_Node>(name, import_basis);
  Mesh mesh(m, name);
  Material material(md);
  node->model.push_back({mesh, material});
  set_parent(node, parent);
  return node;
}
void Scene_Graph::set_parent(weak_ptr<Scene_Graph_Node> p,
                             weak_ptr<Scene_Graph_Node> desired_parent)
{
  shared_ptr<Scene_Graph_Node> current_parent_ptr = p.lock()->parent.lock();
  if (current_parent_ptr)
  {
    for (auto i = current_parent_ptr->children.begin();
         i != current_parent_ptr->children.end(); ++i)
    {
      if (*i == p.lock())
      {
        current_parent_ptr->children.erase(i);
        break;
      }
    }
  }
  shared_ptr<Scene_Graph_Node> desired_parent_ptr = desired_parent.lock();
  if (desired_parent_ptr)
  {
    p.lock()->parent = desired_parent;
    desired_parent_ptr->children.push_back(Node_Ptr(p));
  }
  else
  {
    p.lock()->parent = root;
    root->children.push_back(Node_Ptr(p));
  }
}

shared_ptr<Scene_Graph_Node>
Scene_Graph::add_aiscene(string scene_file_path, const mat4 *import_basis,
                         weak_ptr<Scene_Graph_Node> parent,
                         Material_Descriptor *material_override)
{
  return add_aiscene(load_aiscene(scene_file_path), scene_file_path,
                     import_basis, parent, material_override);
}

shared_ptr<Scene_Graph_Node>
Scene_Graph::add_aiscene(string scene_file_path,
                         Material_Descriptor *material_override)
{
  return add_aiscene(load_aiscene(scene_file_path), scene_file_path, nullptr,
                     weak_ptr<Scene_Graph_Node>(), material_override);
}

shared_ptr<Scene_Graph_Node> Scene_Graph::add_aiscene(
    const aiScene *scene, string scene_file_path, const mat4 *import_basis,
    weak_ptr<Scene_Graph_Node> parent, Material_Descriptor *material_override)
{
  // accumulates as meshes are imported, used along with the scene file path
  // to create a unique_id for the mesh
  Uint32 mesh_num = 0;
  const aiNode *root = scene->mRootNode;

  // create the root node for this scene
  string name =
      string("ROOT FOR: ") + scene_file_path + " " + copy(&root->mName);
  scene_file_path = BASE_MODEL_PATH + scene_file_path;
  shared_ptr<Scene_Graph_Node> new_node = make_shared<Scene_Graph_Node>(
      name, root, import_basis, scene, scene_file_path, &mesh_num,
      material_override);

  set_parent(new_node, parent);

  // add every aiscene child to the new node
  const uint32 num_children = scene->mRootNode->mNumChildren;
  for (uint32 i = 0; i < num_children; ++i)
  {
    const aiNode *node = scene->mRootNode->mChildren[i];
    add_graph_node(node, new_node, import_basis, scene, scene_file_path,
                   &mesh_num, material_override);
  }
  return new_node;
}
void Scene_Graph::visit_nodes(const weak_ptr<Scene_Graph_Node> node_ptr,
                              const mat4 &M, vector<Render_Entity> &accumulator)
{
  shared_ptr<Scene_Graph_Node> entity = node_ptr.lock();
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
  for (auto i = entity->children.begin(); i != entity->children.end(); ++i)
  {
    weak_ptr<Scene_Graph_Node> child = *i;

    visit_nodes(child, STACK, accumulator);
  }
}
void Scene_Graph::visit_nodes_locked_accumulator(
    weak_ptr<Scene_Graph_Node> node_ptr, const mat4 &M,
    vector<Render_Entity> *accumulator, atomic_flag *lock)
{
  shared_ptr<Scene_Graph_Node> entity = node_ptr.lock();

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
    weak_ptr<Scene_Graph_Node> child = entity->children[i];
    visit_nodes_locked_accumulator(child, STACK, accumulator, lock);
  }
}

void Scene_Graph::visit_root_node_base_index(uint32 node_index, uint32 count,
                                             vector<Render_Entity> *accumulator,
                                             atomic_flag *lock)
{
  const Scene_Graph_Node *const entity = root.get();
  ASSERT(entity->name == "SCENE_GRAPH_ROOT");
  ASSERT(entity->position == vec3(0));
  ASSERT(entity->orientation == quat());
  ASSERT(entity->import_basis == mat4(1));

  for (uint32 i = node_index; i < node_index + count; ++i)
  {
    weak_ptr<Scene_Graph_Node> child = entity->children[i];
    visit_nodes_locked_accumulator(child, mat4(1), accumulator, lock);
  }
}

void Scene_Graph::visit_root_node_base_index_alt(
    uint32 node_index, uint32 count, vector<Render_Entity> *accumulator,
    atomic_flag *lock)
{
  Scene_Graph_Node *const entity = root.get();
  ASSERT(entity->name == "SCENE_GRAPH_ROOT");
  ASSERT(entity->position == vec3(0));
  ASSERT(entity->orientation == quat(0, 0, 0, 1));
  ASSERT(entity->import_basis == mat4(1));

  vector<Render_Entity> my_accumulator;
  my_accumulator.reserve(count * 1000);
  for (uint32 i = node_index; i < node_index + count; ++i)
  {
    weak_ptr<Scene_Graph_Node> child = entity->children[i];
    visit_nodes(child, mat4(1), my_accumulator);
  }
  while (lock->test_and_set())
  {
  }
  copy(my_accumulator.begin(), my_accumulator.end(),
       back_inserter(*accumulator));
  lock->clear();
}
vector<Render_Entity> Scene_Graph::visit_nodes_async_start()
{
  vector<Render_Entity> accumulator;
  accumulator.reserve(uint32(last_accumulator_size * 1.5));

  const uint32 root_children_count = root->children.size();
  const uint32 thread_count = 4;
  const uint32 nodes_per_thread = root_children_count / thread_count;
  const uint32 leftover_nodes = root_children_count % thread_count;

  PERF_TIMER.start();
  // todo the refactoring into class member function(or regression to W7 from
  // W10)
  // or other change killed the performance here
  thread threads[thread_count];
  atomic_flag lock = ATOMIC_FLAG_INIT;
  for (uint32 i = 0; i < thread_count; ++i)
  {
    const uint32 base_index = i * nodes_per_thread;
    auto fp = [this, base_index, nodes_per_thread, &accumulator, &lock] {
      this->visit_root_node_base_index(base_index, nodes_per_thread,
                                       &accumulator, &lock);
    };
    threads[i] = thread(fp);
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
vector<Render_Entity> Scene_Graph::visit_nodes_st_start()
{
  vector<Render_Entity> accumulator;
  accumulator.reserve(uint32(last_accumulator_size * 1.5));
  visit_nodes(root, mat4(1), accumulator);
  last_accumulator_size = accumulator.size();
  return accumulator;
}

shared_ptr<Scene_Graph_Node> Scene_Graph::add_primitive_mesh(
    Mesh_Primitive p, string name, Material_Descriptor m,
    weak_ptr<Scene_Graph_Node> parent, const mat4 *import_basis)
{
  Mesh mesh(p, name);
  Material material(m);
  shared_ptr<Scene_Graph_Node> new_node =
      make_shared<Scene_Graph_Node>(name, import_basis);
  new_node->model.push_back({mesh, material});
  set_parent(new_node, parent);
  return new_node;
}
