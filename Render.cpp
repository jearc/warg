#include <GL/glew.h>
#include <SDL.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "Globals.h"
#include "Mesh_Loader.h"
#include "Render.h"
#include "Shader.h"
#include "Timer.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sstream>
using namespace glm;

// our frag shader output attachment points for deferred rendering
#define DIFFUSE_TARGET GL_COLOR_ATTACHMENT0
#define NORMAL_TARGET GL_COLOR_ATTACHMENT1
#define POSITION_TARGET GL_COLOR_ATTACHMENT2
#define DEPTH_TARGET GL_COLOR_ATTACHMENT3

const GLenum RENDER_TARGETS[] = {DIFFUSE_TARGET, NORMAL_TARGET, POSITION_TARGET,
                                 DEPTH_TARGET};
#define TARGET_COUNT sizeof(RENDER_TARGETS) / sizeof(GLenum)


static Timer FRAME_TIMER = Timer(60);
static Timer SWAP_TIMER = Timer(60);
static GLuint TARGET_FRAMEBUFFER = 0; //fbo that gets rendered to
static GLuint COLOR_TARGET_TEXTURE = 0; //color texture that is bound to target framebuffer
static GLuint DEPTH_TARGET_TEXTURE = 0; //depth texture that is bound to target framebuffer
static GLuint PREV_COLOR_TARGET = 0; //color texture from previous frame
static bool PREV_COLOR_TARGET_MISSING = true;
static GLuint INSTANCE_MVP_BUFFER = 0; //buffer object holding MVP matrices
static GLuint INSTANCE_MODEL_BUFFER = 0;//buffer object holding model matrices
static Mesh QUAD;
static Shader TEMPORALAA;
static Shader PASSTHROUGH;
static bool INIT = false;
static std::unordered_map<std::string, std::weak_ptr<Mesh_Handle>> MESH_CACHE;
static std::unordered_map<std::string, std::weak_ptr<Texture_Handle>> TEXTURE_CACHE;
void INIT_RENDERER()
{
  if (INIT) return;
  INIT = true;

  QUAD = Mesh(Mesh_Primitive::plane, "Renderer's plane");
  TEMPORALAA = Shader("passthrough.vert", "TemporalAA.frag");
  PASSTHROUGH = Shader("passthrough.vert", "passthrough.frag");


  // instancing MVP Matrix buffer init
  const GLuint mat4_size = sizeof(GLfloat) * 4 * 4;
  const GLuint instance_buffer_size = MAX_INSTANCE_COUNT * mat4_size;

  glGenBuffers(1, &INSTANCE_MVP_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MVP_BUFFER);
  glBufferData(GL_ARRAY_BUFFER, instance_buffer_size, (void *)0,
    GL_DYNAMIC_DRAW);

  // instancing Model Matrix buffer init
  glGenBuffers(1, &INSTANCE_MODEL_BUFFER);
  glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MODEL_BUFFER);
  glBufferData(GL_ARRAY_BUFFER, instance_buffer_size, (void *)0,
    GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);




}
void CLEANUP_RENDERER()
{
  QUAD = Mesh();
  TEMPORALAA = Shader();
  PASSTHROUGH = Shader();

  glDeleteFramebuffers(1, &TARGET_FRAMEBUFFER);
  glDeleteTextures(1, &COLOR_TARGET_TEXTURE);
  glDeleteTextures(1, &PREV_COLOR_TARGET);
  glDeleteRenderbuffers(1, &DEPTH_TARGET_TEXTURE);
  glDeleteBuffers(1, &INSTANCE_MVP_BUFFER);
  glDeleteBuffers(1, &INSTANCE_MODEL_BUFFER);
}

void check_and_clear_expired_textures()
{
  auto it = TEXTURE_CACHE.begin();
  while (it != TEXTURE_CACHE.end())
  {
    auto texture_handle = *it;
    const std::string *path = &texture_handle.first;
    struct stat attr;
    stat(path->c_str(), &attr);
    time_t t = attr.st_mtime;

    auto ptr = texture_handle.second.lock();
    if (ptr) // texture could exist but not yet be loaded
    {
      time_t t1 = ptr.get()->file_mod_t;
      if (t1 != t)
      {
        it = TEXTURE_CACHE.erase(it);
        continue;
      }
    }
    ++it;
  }
}

enum Texture_Location
{
  albedo,
  specular,
  normal,
  emissive,
  roughness
};

Texture::Texture() { file_path = ERROR_TEXTURE_PATH; }
Texture_Handle::~Texture_Handle()
{
  glDeleteTextures(1, &texture);
  texture = 0;
}
Texture::Texture(std::string path)
{
  path = fix_filename(path);
  if (path.size() == 0)
  {
    file_path = ERROR_TEXTURE_PATH;
    return;
  }
  if (path.substr(0, 6) == "color(")
  { // custom color
    file_path = path;
    return;
  }

  if (path.find_last_of("/") == path.npos)
  { // no specified directory, so use base path
    file_path = BASE_TEXTURE_PATH + path;
    return;
  }
  else
  { // assimp imported model or user specified a directory
    file_path = path;
    return;
  }
}

void Texture::load()
{
#if !SHOW_ERROR_TEXTURE
  if (file_path == ERROR_TEXTURE_PATH || file_path == BASE_TEXTURE_PATH)
  { //the file path doesnt point to a valid texture, or points to the error texture
    //a null texture here shows as 0,0,0,0 in the shader
    texture = nullptr;
    return;
  }
#endif
  auto ptr = TEXTURE_CACHE[file_path].lock();
  if (ptr)
  {
    texture = ptr;
    return;
  }
  texture = std::make_shared<Texture_Handle>();
  TEXTURE_CACHE[file_path] = texture;
  int32 width, height, n;
  if (file_path.substr(0, 6) == "color(")
  {
    width = height = 1;
    Uint32 color = string_to_color(file_path);
    glGenTextures(1, &texture->texture);
    glBindTexture(GL_TEXTURE_2D, texture->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
      GL_UNSIGNED_INT_8_8_8_8, &color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
      GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    return;
  }
  auto *data = stbi_load(file_path.c_str(), &width, &height, &n, 4);

  if (!data)
  {//error loading file...
#if DYNAMIC_TEXTURE_RELOADING
    // retry next frame
    set_message("Warning: missing texture:" + file_path);
    texture = nullptr;
    return;
#else
    if (!data)
    {//no dynamic texture reloading, so log fail and load the error texture
      set_message("STBI failed to find or load texture: " + file_path);
      if (file_path == ERROR_TEXTURE_PATH)
      {
        texture = nullptr;
        return;
      }
      file_path = ERROR_TEXTURE_PATH;
      load();
      return;
    }
#endif
  }


  set_message("Texture load cache miss. Texture from disk: ", file_path, 1.0);
  // TODO: optimize the texture storage types to save GPU memory
  // currently  everything is stored with RGBA
  // could pack maps together like: RGBA with diffuse as RGB and A as
  // ambient_occlusion

  struct stat attr;
  stat(file_path.c_str(), &attr);
  texture.get()->file_mod_t = attr.st_mtime;
  glGenTextures(1, &texture->texture);
  glBindTexture(GL_TEXTURE_2D, texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
  glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(data);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::bind(const char *name, GLuint binding, Shader &shader)
{
#if DYNAMIC_TEXTURE_RELOADING
  load();
#endif
  GLuint u = glGetUniformLocation(shader.program->program, name);

  glUniform1i(u, binding);
  glActiveTexture(GL_TEXTURE0 + (GLuint)binding);
  glBindTexture(GL_TEXTURE_2D, texture ? texture->texture : 0);
  if (!texture)
  {
    // std::cout << "Warning: Null texture pointer with name: " << name
    //           << " and binding: " << binding << "\n";
  }
}

Mesh::Mesh() {}
Mesh::Mesh(Mesh_Primitive p, std::string mesh_name) : name(mesh_name)
{
  unique_identifier = identifier_for_primitive(p);
  auto ptr = MESH_CACHE[unique_identifier].lock();
  if (ptr)
  {
    //assert that the data is actually exactly the same
    mesh = ptr;
    return;
  }
  MESH_CACHE[unique_identifier] = mesh = upload_data(load_mesh(p));
}
Mesh::Mesh(Mesh_Data data, std::string mesh_name)
    : name(mesh_name)
{
  unique_identifier = data.unique_identifier;
  auto ptr = MESH_CACHE[unique_identifier].lock();
  if (ptr)
  {
    //assert that the data is actually exactly the same
    mesh = ptr;
    return;
  }
  MESH_CACHE[unique_identifier] = mesh = upload_data(data);
}
Mesh::Mesh(const aiMesh *aimesh,std::string unique_identifier)
{
  ASSERT(aimesh);
  this->unique_identifier = unique_identifier;
  auto ptr = MESH_CACHE[unique_identifier].lock();
  if (ptr)
  {
    //assert that the data is actually exactly the same
    mesh = ptr;
    return;
  }
  MESH_CACHE[unique_identifier] = mesh = upload_data(load_mesh(aimesh, unique_identifier));
}


void Mesh::assign_instance_buffers(GLuint instance_MVP_Buffer,
                                   GLuint instance_Model_Buffer, Shader &shader)
{

  if (!instance_buffers_set)
  {
    instance_buffers_set = true;
    GLint current_vao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &current_vao);
    ASSERT(mesh);
    ASSERT(current_vao == mesh->vao);
    const GLuint mat4_size = sizeof(GLfloat) * 4 * 4;
    // shader attribute locations
    // 4 locations for mat4
    int32 loc = glGetAttribLocation(shader.program->program, "instanced_MVP");
    if (loc != -1)
    {
      GLuint loc1 = loc + 0;
      GLuint loc2 = loc + 1;
      GLuint loc3 = loc + 2;
      GLuint loc4 = loc + 3;
      glEnableVertexAttribArray(loc1);
      glEnableVertexAttribArray(loc2);
      glEnableVertexAttribArray(loc3);
      glEnableVertexAttribArray(loc4);
      glBindBuffer(GL_ARRAY_BUFFER, instance_MVP_Buffer);
      glVertexAttribPointer(loc1, 4, GL_FLOAT, GL_FALSE, mat4_size,
                            (void *)(0));
      glVertexAttribPointer(loc2, 4, GL_FLOAT, GL_FALSE, mat4_size,
                            (void *)(sizeof(GLfloat) * 4));
      glVertexAttribPointer(loc3, 4, GL_FLOAT, GL_FALSE, mat4_size,
                            (void *)(sizeof(GLfloat) * 8));
      glVertexAttribPointer(loc4, 4, GL_FLOAT, GL_FALSE, mat4_size,
                            (void *)(sizeof(GLfloat) * 12));
      glVertexAttribDivisor(loc1, 1);
      glVertexAttribDivisor(loc2, 1);
      glVertexAttribDivisor(loc3, 1);
      glVertexAttribDivisor(loc4, 1);
    }

    loc = glGetAttribLocation(shader.program->program, "instanced_model");
    if (loc != -1)
    {
      GLuint loc_1 = loc + 0;
      GLuint loc_2 = loc + 1;
      GLuint loc_3 = loc + 2;
      GLuint loc_4 = loc + 3;
      glEnableVertexAttribArray(loc_1);
      glEnableVertexAttribArray(loc_2);
      glEnableVertexAttribArray(loc_3);
      glEnableVertexAttribArray(loc_4);
      glBindBuffer(GL_ARRAY_BUFFER, instance_Model_Buffer);
      glVertexAttribPointer(loc_1, 4, GL_FLOAT, GL_FALSE, mat4_size,
                            (void *)(0));
      glVertexAttribPointer(loc_2, 4, GL_FLOAT, GL_FALSE, mat4_size,
                            (void *)(sizeof(GLfloat) * 4));
      glVertexAttribPointer(loc_3, 4, GL_FLOAT, GL_FALSE, mat4_size,
                            (void *)(sizeof(GLfloat) * 8));
      glVertexAttribPointer(loc_4, 4, GL_FLOAT, GL_FALSE, mat4_size,
                            (void *)(sizeof(GLfloat) * 12));
      glVertexAttribDivisor(loc_1, 1);
      glVertexAttribDivisor(loc_2, 1);
      glVertexAttribDivisor(loc_3, 1);
      glVertexAttribDivisor(loc_4, 1);
    }
  }
}


void Mesh::bind_to_shader(Shader &shader)
{
  ASSERT(mesh);
  int32 loc = glGetAttribLocation(shader.program->program, "position");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->position_buffer);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(loc);
  }
  loc = glGetAttribLocation(shader.program->program, "normal");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->normal_buffer);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(loc);
  }
  loc = glGetAttribLocation(shader.program->program, "uv");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->uv_buffer);
    glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(float32) * 2, 0);
    glEnableVertexAttribArray(loc);
  }
  loc = glGetAttribLocation(shader.program->program, "tangent");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->tangents_buffer);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(loc);
  }
  loc = glGetAttribLocation(shader.program->program, "bitangent");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->bitangents_buffer);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(loc);
  }
}

std::shared_ptr<Mesh_Handle> Mesh::upload_data(const Mesh_Data& mesh_data)
{
  std::shared_ptr<Mesh_Handle> mesh = std::make_shared<Mesh_Handle>();
  mesh->data = mesh_data;

  if (sizeof(decltype(mesh_data.indices)::value_type) != sizeof(uint32))
  {
    set_message("Mesh::upload_data error: render() assumes index type to be 32 bits");
    ASSERT(0);
  }

  check_gl_error();
  mesh->indices_buffer_size = mesh_data.indices.size();
  glGenVertexArrays(1, &mesh->vao);
  glBindVertexArray(mesh->vao);
  set_message("create_vao", "created vao: " + std::to_string(mesh->vao), 1);
  glGenBuffers(1, &mesh->position_buffer);
  glGenBuffers(1, &mesh->normal_buffer);
  glGenBuffers(1, &mesh->indices_buffer);
  glGenBuffers(1, &mesh->uv_buffer);
  glGenBuffers(1, &mesh->tangents_buffer);
  glGenBuffers(1, &mesh->bitangents_buffer);

  uint32 positions_buffer_size = mesh_data.positions.size();
  uint32 normal_buffer_size = mesh_data.normals.size();
  uint32 uv_buffer_size = mesh_data.texture_coordinates.size();
  uint32 tangents_size = mesh_data.tangents.size();
  uint32 bitangents_size = mesh_data.bitangents.size();
  uint32 indices_buffer_size = mesh_data.indices.size();

  ASSERT(all_equal(positions_buffer_size, normal_buffer_size, uv_buffer_size,
                   tangents_size, bitangents_size));

  // positions
  uint32 buffer_size = mesh_data.positions.size() *
                       sizeof(decltype(mesh_data.positions)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->position_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.positions[0],
               GL_STATIC_DRAW);

  // normals
  buffer_size = mesh_data.normals.size() *
                sizeof(decltype(mesh_data.normals)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->normal_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.normals[0],
               GL_STATIC_DRAW);

  // texture_coordinates
  buffer_size = mesh_data.texture_coordinates.size() *
                sizeof(decltype(mesh_data.texture_coordinates)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.texture_coordinates[0],
               GL_STATIC_DRAW);

  // indices
  buffer_size = mesh_data.indices.size() *
                sizeof(decltype(mesh_data.indices)::value_type);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer_size, &mesh_data.indices[0],
               GL_STATIC_DRAW);

  // tangents
  buffer_size = mesh_data.tangents.size() *
                sizeof(decltype(mesh_data.tangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->tangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.tangents[0],
               GL_STATIC_DRAW);

  // bitangents
  buffer_size = mesh_data.bitangents.size() *
                sizeof(decltype(mesh_data.bitangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->bitangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.bitangents[0],
               GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  return mesh;
}

Material::Material() {}
Material::Material(Material_Descriptor m) 
{ 
  load(m);
}
Material::Material(aiMaterial *ai_material, std::string working_directory, Material_Descriptor* material_override)
{
  ASSERT(ai_material);
  const int albedo_n = ai_material->GetTextureCount(aiTextureType_DIFFUSE);
  const int specular_n = ai_material->GetTextureCount(aiTextureType_SPECULAR);
  const int emissive_n = ai_material->GetTextureCount(aiTextureType_EMISSIVE);
  const int normal_n = ai_material->GetTextureCount(aiTextureType_NORMALS);
  const int roughness_n = ai_material->GetTextureCount(aiTextureType_SHININESS);

  // unused:
  aiTextureType_HEIGHT;
  aiTextureType_OPACITY;
  aiTextureType_DISPLACEMENT;
  aiTextureType_AMBIENT;
  aiTextureType_LIGHTMAP;

  Material_Descriptor m;
  aiString name;
  ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &name);
  m.albedo = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_SPECULAR, 0, &name);
  m.specular = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_EMISSIVE, 0, &name);
  m.emissive = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_NORMALS, 0, &name);
  m.normal = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_LIGHTMAP, 0, &name);
  m.ambient_occlusion = copy(&name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_SHININESS, 0, &name);
  m.roughness = copy(&name);
  name.Clear();

  if (albedo_n)
    m.albedo = working_directory + m.albedo;
  if (specular_n)
    m.specular = working_directory + m.specular;
  if (emissive_n)
    m.emissive = working_directory + m.emissive;
  if (normal_n)
    m.normal = working_directory + m.normal;
  if (roughness_n)
    m.roughness = working_directory + m.roughness;  

  if (material_override)
  {
    if (material_override->albedo != "")
      m.albedo = material_override->albedo;
    if (material_override->roughness != "")
      m.roughness = material_override->roughness;
    if (material_override->specular != "")
      m.specular = material_override->specular;
    if (material_override->metalness != "")
      m.metalness = material_override->metalness;
    if (material_override->tangent != "")
      m.tangent = material_override->tangent;
    if (material_override->normal != "")
      m.normal = material_override->normal;
    if (material_override->ambient_occlusion != "")
      m.ambient_occlusion = material_override->ambient_occlusion;
    if (material_override->emissive != "")
      m.emissive = material_override->emissive;
    m.vertex_shader = material_override->vertex_shader;
    m.frag_shader = material_override->frag_shader;
    m.backface_culling = material_override->backface_culling;
    m.has_transparency = material_override->has_transparency;
    m.uv_scale = material_override->uv_scale;
  }
  load(m);
}
void Material::load(Material_Descriptor m)
{
  this->m = m;
  albedo = Texture(m.albedo);
  specular_color = Texture(m.specular);
  normal = Texture(m.normal);
  emissive = Texture(m.emissive);
  roughness = Texture(m.roughness);
  shader = Shader(m.vertex_shader, m.frag_shader);
  uv_scale = m.uv_scale;
}
void Material::bind()
{
  if (m.backface_culling)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  albedo.bind("albedo", 0, shader);
  specular_color.bind("specular_color", 1, shader);
  normal.bind("normal", 2, shader);
  emissive.bind("emissive", 3, shader);
  roughness.bind("roughness", 4, shader);
}
void Material::unbind_textures()
{
  glActiveTexture(GL_TEXTURE0 + Texture_Location::albedo);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::specular);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::normal);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::emissive);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0 + Texture_Location::roughness);
  glBindTexture(GL_TEXTURE_2D, 0);
}

bool Light::operator==(const Light &rhs) const
{
  bool b1 = position == rhs.position;
  bool b2 = direction == rhs.direction;
  bool b3 = color == rhs.color;
  bool b4 = attenuation == rhs.attenuation;
  bool b5 = ambient == rhs.ambient;
  bool b6 = cone_angle == rhs.cone_angle;
  bool b7 = type == rhs.type;
  return b1 & b2 & b3 & b4 & b5 & b6 & b7;
}

bool Light_Array::operator==(const Light_Array &rhs)
{
  if (light_count != rhs.light_count)
    return false;
  if (lights != rhs.lights)
  {
    return false;
  }
  if (additional_ambient != rhs.additional_ambient)
  {
    return false;
  }
  return true;
}

Render_Entity::Render_Entity(Mesh *mesh, Material *material, Light_Array lights,
                             mat4 world_to_model)
    : mesh(mesh), material(material), lights(lights),
      transformation(world_to_model)
{
  ASSERT(mesh);
  ASSERT(material);
}
 

Render::Render(SDL_Window *window, ivec2 window_size)
{
  this->window = window;
  this->window_size = window_size;
  SDL_DisplayMode current;
  SDL_GetCurrentDisplayMode(0, &current);
  target_frame_time = 1.0f / (float32)current.refresh_rate;
  set_vfov(vfov);
  init_render_targets();
  FRAME_TIMER.start();
}


void set_uniform_lights(Shader &shader, Light_Array &lights)
{
  ASSERT(lights.lights.size() == MAX_LIGHTS);
  uint32 location = -1;
// todo: this is horrible. do something much better than this - precompute
// all these godawful strings and just select them
#define s std::string
#define ts std::to_string

  for (int i = 0; i < MAX_LIGHTS; ++i)
  {
    shader.set_uniform((s("lights[") + ts(i) + s("].position")).c_str(),
                       lights.lights[i].position);
    shader.set_uniform((s("lights[") + ts(i) + s("].direction")).c_str(),
                       lights.lights[i].direction);
    shader.set_uniform((s("lights[") + ts(i) + s("].color")).c_str(),
                       lights.lights[i].color);
    shader.set_uniform((s("lights[") + ts(i) + s("].attenuation")).c_str(),
                       lights.lights[i].attenuation);
    vec3 ambient = lights.lights[i].ambient * lights.lights[i].color;
    shader.set_uniform((s("lights[") + ts(i) + s("].ambient")).c_str(),
      ambient);
    shader.set_uniform((s("lights[") + ts(i) + s("].cone_angle")).c_str(),
                       lights.lights[i].cone_angle);
    shader.set_uniform((s("lights[") + ts(i) + s("].type")).c_str(),
                       (int32)lights.lights[i].type);
  }
  shader.set_uniform("number_of_lights", (int32)lights.light_count);
  shader.set_uniform("additional_ambient", lights.additional_ambient);

#undef s
#undef ts
}

void Render::render(float64 state_time)
{
#if DYNAMIC_FRAMERATE_TARGET
  dynamic_framerate_target();
#endif
#if DYNAMIC_TEXTURE_RELOADING
  check_and_clear_expired_textures();
#endif

  float32 time = (float32)get_real_time();
  float64 t = (time - state_time) / dt;
  glViewport(0, 0, size.x, size.y);
  glBindFramebuffer(GL_FRAMEBUFFER, TARGET_FRAMEBUFFER);
  glFramebufferTexture(GL_FRAMEBUFFER, DIFFUSE_TARGET, COLOR_TARGET_TEXTURE, 0);

  glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  for (auto &entity : render_instances)
  {
    check_gl_error();
    Shader &shader = entity.material->shader;
    shader.use();
    entity.material->bind();
    ASSERT(entity.mesh);
    int vao = entity.mesh->get_vao();
    set_message("vao bind", "binding vao:" + std::to_string(vao), 1);
    glBindVertexArray(vao); 
    entity.mesh->bind_to_shader(shader);
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    shader.set_uniform("uv_scale", entity.material->uv_scale);
    set_uniform_lights(shader, entity.lights);
    //// verify sizes of data, mat4 floats
    ASSERT(entity.Model_Matrices.size() > 0);
    ASSERT(entity.MVP_Matrices.size() == entity.Model_Matrices.size());
    ASSERT(sizeof(decltype(entity.Model_Matrices[0])) ==
           sizeof(decltype(entity.MVP_Matrices[0])));
    ASSERT(sizeof(decltype(entity.MVP_Matrices[0][0][0])) ==
           sizeof(GLfloat)); // buffer init code assumes these
    ASSERT(sizeof(decltype(entity.MVP_Matrices[0])) == sizeof(mat4));

    uint32 num_instances = entity.MVP_Matrices.size();
    uint32 buffer_size = num_instances * sizeof(mat4);
    entity.mesh->assign_instance_buffers(INSTANCE_MVP_BUFFER,
                                         INSTANCE_MODEL_BUFFER, shader);
    glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MVP_BUFFER);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size,
                    &entity.MVP_Matrices[0][0][0]);
    glBindBuffer(GL_ARRAY_BUFFER, INSTANCE_MODEL_BUFFER);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size,
                    &entity.Model_Matrices[0][0][0]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->get_indices_buffer());  
    glDrawElementsInstanced(GL_TRIANGLES, entity.mesh->get_indices_buffer_size(),
                            GL_UNSIGNED_INT, (void *)0, num_instances);

    entity.material->unbind_textures();
  }
  check_gl_error();

  mat4 o =
      ortho(0.0f, (float32)window_size.x, 0.0f, (float32)window_size.y, 0.1f,
            100.0f) *
      translate(vec3(vec2(0.5f * window_size.x, 0.5f * window_size.y), -1)) *
      scale(vec3(window_size, 1));

  if (use_txaa)
  {
    // TODO: implement motion vector vertex attribute

    // render to main framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_size.x, window_size.y);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    TEMPORALAA.use();
    QUAD.bind_to_shader(TEMPORALAA);
    GLuint u = glGetUniformLocation(TEMPORALAA.program->program, "current");
    glUniform1i(u, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, COLOR_TARGET_TEXTURE);
    GLuint u2 = glGetUniformLocation(TEMPORALAA.program->program, "previous");
    glUniform1i(u2, 1);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,
                  PREV_COLOR_TARGET_MISSING ? COLOR_TARGET_TEXTURE : PREV_COLOR_TARGET);
    TEMPORALAA.set_uniform("transform", o);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(), GL_UNSIGNED_INT,
                   (void *)0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFinish(); // intent is to time just the swap itself
    FRAME_TIMER.stop();
    SWAP_TIMER.start();
    SDL_GL_SwapWindow(window);
    glFinish();
    SWAP_TIMER.stop();
    FRAME_TIMER.start();

    // copy this frame to previous framebuffer
    // so it will be usable next frame, when its the old frame

    // assign previous_color as the target to overwrite
    glViewport(0, 0, size.x, size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, TARGET_FRAMEBUFFER);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         PREV_COLOR_TARGET, 0);
    PASSTHROUGH.use();
    QUAD.bind_to_shader(PASSTHROUGH);
    // sample the current color_target as source
    GLuint u3 = glGetUniformLocation(PASSTHROUGH.program->program, "albedo");
    glUniform1i(u3, Texture_Location::albedo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, COLOR_TARGET_TEXTURE);
    PASSTHROUGH.set_uniform("transform", o);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(), GL_UNSIGNED_INT,
      (void *)0);

    PREV_COLOR_TARGET_MISSING = false;
    // place color_target back in target_fbo
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, COLOR_TARGET_TEXTURE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    txaa_jitter = get_next_TXAA_sample();
  }
  else
  {
    // render to main framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_size.x, window_size.y);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    PASSTHROUGH.use();
    QUAD.bind_to_shader(PASSTHROUGH);
    GLuint u = glGetUniformLocation(PASSTHROUGH.program->program, "albedo");
    glUniform1i(u, Texture_Location::albedo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, COLOR_TARGET_TEXTURE);
    PASSTHROUGH.set_uniform("transform", o);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, QUAD.get_indices_buffer());
    glDrawElements(GL_TRIANGLES, QUAD.get_indices_buffer_size(), GL_UNSIGNED_INT,
                   (void *)0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glFinish(); // intent is to time just the swap itself
    FRAME_TIMER.stop();
    SWAP_TIMER.start();
    SDL_GL_SwapWindow(window);
    glFinish();
    SWAP_TIMER.stop();
    FRAME_TIMER.start();
  }
  frame_count += 1;
}

void Render::resize_window(ivec2 window_size)
{
  this->window_size = window_size;
  set_vfov(vfov);
  ASSERT(0); // not yet implemented
}

void Render::set_render_scale(float32 scale)
{
  if (scale < 0.1f)
    scale = 0.1f;
  if (scale > 2.0f)
    scale = 2.0f;
  if (render_scale == scale)
    return;
  render_scale = scale;
  init_render_targets();
  time_of_last_scale_change = get_real_time();
}

void Render::set_camera(vec3 pos, vec3 camera_gaze_dir)
{
  vec3 p = pos + camera_gaze_dir;
  camera_position = pos;
  camera = glm::lookAt(pos, p, {0, 0, 1});
}
void Render::set_camera_gaze(vec3 pos, vec3 p)
{
  camera_position = pos;
  camera = glm::lookAt(pos, p, {0, 0, 1});
}

void Render::set_vfov(float32 vfov)
{
  const float32 aspect = (float32)window_size.x / (float32)window_size.y;
  const float32 znear = 0.1f;
  const float32 zfar = 1000;
  projection = glm::perspective(radians(vfov), aspect, znear, zfar);
}

void Render::set_render_entities(std::vector<Render_Entity> render_entities)
{
  // todo: save previous update's entities
  // give each entity a unique ID, sort by ID, compare
  // extrapolate positions for current frame with
  // lerp(current, current + (current-prev), t)
  render_instances.clear();
  render_instances.reserve(render_entities.size());

  for (uint32 i = 0; i < render_entities.size(); ++i)
  {
    Render_Entity *entity = &render_entities[i];
    bool found = false;
    for (uint32 j = 0; j < render_instances.size(); ++j)
    { // check every instance for a match
      Render_Instance *instance = &render_instances[j];
      if ((instance->mesh == entity->mesh) &&
          (instance->material == entity->material))
      {
        ASSERT(instance->lights.lights == entity->lights.lights);
        ASSERT(instance->lights.light_count == entity->lights.light_count);
        // varying lights for an instance not (yet?) supported

        // we actually found an object that is drawn more than once
        // odds are, an instanced mesh will have more than just a few instances
        instance->MVP_Matrices.reserve(MAX_INSTANCE_COUNT);
        instance->Model_Matrices.reserve(MAX_INSTANCE_COUNT);
        // TODO: pack these together in one buffer
        // with a stride for cache locality
         mat4 mvp = projection * camera * entity->transformation;
        instance->MVP_Matrices.push_back(mvp);
        instance->Model_Matrices.push_back(entity->transformation);
        ASSERT(instance->MVP_Matrices.size() <= MAX_INSTANCE_COUNT);
        ASSERT(instance->Model_Matrices.size() <= MAX_INSTANCE_COUNT);
        found = true;
        break;
      }
    }
    if (!found)
    { // add a new instance
      render_instances.emplace_back();
      Render_Instance *new_instance = &render_instances.back();
      new_instance->lights = entity->lights;
      new_instance->mesh = entity->mesh;
      new_instance->material = entity->material;
      mat4 mvp = projection * camera * entity->transformation;
      new_instance->MVP_Matrices.push_back(mvp);
      new_instance->Model_Matrices.push_back(entity->transformation);
    }
  }
}

void Render::init_render_targets()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &TARGET_FRAMEBUFFER);
  glDeleteTextures(1, &COLOR_TARGET_TEXTURE);
  glDeleteTextures(1, &PREV_COLOR_TARGET);
  glDeleteRenderbuffers(1, &DEPTH_TARGET_TEXTURE);

  size = ivec2(render_scale * window_size.x, render_scale * window_size.y);
  glGenFramebuffers(1, &TARGET_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, TARGET_FRAMEBUFFER);

  glGenTextures(1, &COLOR_TARGET_TEXTURE);
  glBindTexture(GL_TEXTURE_2D, COLOR_TARGET_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  glGenTextures(1, &PREV_COLOR_TARGET);
  glBindTexture(GL_TEXTURE_2D, PREV_COLOR_TARGET);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  glFramebufferTexture(GL_FRAMEBUFFER, DIFFUSE_TARGET, COLOR_TARGET_TEXTURE, 0);
  glDrawBuffers(TARGET_COUNT, RENDER_TARGETS);
  glGenRenderbuffers(1, &DEPTH_TARGET_TEXTURE);
  glBindRenderbuffer(GL_RENDERBUFFER, DEPTH_TARGET_TEXTURE);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size.x, size.y);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, DEPTH_TARGET_TEXTURE);

  PREV_COLOR_TARGET_MISSING = true;

  set_message("Init render targets: ", std::to_string(TARGET_FRAMEBUFFER) + " " +
    std::to_string(PREV_COLOR_TARGET) + " " +
    std::to_string(DEPTH_TARGET_TEXTURE));
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Render::dynamic_framerate_target()
{
  // early out if we don't have enough samples to go by
  const uint32 min_samples = 5;
  // percent of frame+swap samples must be > target_time to downscale
  const float32 reduce_threshold = 0.35f;
  // render scale multiplication factors when changing resolution
  const float32 reduction_factor = 0.85f;
  const float32 increase_factor = 1.08f;
  // gives a little extra time before swap+frame > 1/hz counts as a missed vsync
  // not entirely sure why this is necessary, but at 0 swap+frame is almost
  // always > 1/hz
  // driver overhead and/or timer overhead inaccuracies
  const float64 epsilon = 0.005;
  // if swap alone took longer than this, we should increase the resolution
  // because we probably had a lot of extra gpu time because this blocked for so
  // long
  // not sure if there's a better way to find idle gpu use
  const float64 longest_swap_allowable = target_frame_time * .35;

  if (FRAME_TIMER.sample_count() < min_samples ||
      SWAP_TIMER.sample_count() < min_samples)
  { // don't adjust until we have good avg frame time data
    return;
  }
  // ALL time other than swap time
  const float64 moving_avg = FRAME_TIMER.moving_average();
  const float64 last_frame = FRAME_TIMER.get_last();

  // ONLY swap time
  const float64 last_swap = SWAP_TIMER.get_last();
  const float64 swap_avg = SWAP_TIMER.moving_average();

  auto frame_times = FRAME_TIMER.get_times();
  auto swap_times = SWAP_TIMER.get_times();
  ASSERT(frame_times.size() == swap_times.size());
  uint32 num_high_frame_times = 0;
  for (uint32 i = 0; i < frame_times.size(); ++i)
  {
    const float64 swap = swap_times[i];
    const float64 frame = frame_times[i];
    const bool high = swap + frame > target_frame_time + epsilon;
    num_high_frame_times += int32(high);
  }
  float32 percent_high = float32(num_high_frame_times) / frame_times.size();

  if (percent_high > reduce_threshold)
  {
    set_message("Number of missed frames over threshold. Reducing resolution.: ",std::to_string(percent_high)+" "+std::to_string(target_frame_time));
    set_render_scale(render_scale * reduction_factor);

    // we need to clear the timers because the render scale change
    // means the old times arent really valid anymore - different res
    // frame_timer is currently running so we
    // must get the time it logged for the current
    // frame start, and put it back when we restart
    uint64 last_start = FRAME_TIMER.get_begin();
    SWAP_TIMER.clear_all();
    FRAME_TIMER.clear_all();
    FRAME_TIMER.start(last_start);
  }
  else if (swap_avg > longest_swap_allowable)
  {
    if (render_scale != 2.0) // max scale 2.0
    {
      float64 change_rate = .45;
      if (get_real_time() < time_of_last_scale_change + change_rate)
      { // increase resolution slowly
        return;
      }
      set_message("High avg swap. Increasing resolution.",std::to_string(swap_avg)+" "+std::to_string(target_frame_time));
      set_render_scale(render_scale * increase_factor);
      uint64 last_start = FRAME_TIMER.get_begin();
      SWAP_TIMER.clear_all();
      FRAME_TIMER.clear_all();
      FRAME_TIMER.start(last_start);
    }
  }
}

mat4 Render::get_next_TXAA_sample()
{
  vec2 translation;
  if (jitter_switch)
    translation = vec2(0.5, 0.5);
  else
    translation = vec2(-0.5, -0.5);

  mat4 result = glm::translate(vec3(translation / vec2(size), 0));
  jitter_switch = !jitter_switch;
  return result;
}

Mesh_Handle::~Mesh_Handle()
{
  glDeleteBuffers(1, &position_buffer);
  glDeleteBuffers(1, &normal_buffer);
  glDeleteBuffers(1, &uv_buffer);
  glDeleteBuffers(1, &tangents_buffer);
  glDeleteBuffers(1, &bitangents_buffer);
  glDeleteBuffers(1, &indices_buffer);
  glDeleteVertexArrays(1, &vao);
  vao = 0;
  position_buffer = 0;
  normal_buffer = 0;
  uv_buffer = 0;
  tangents_buffer = 0;
  bitangents_buffer = 0;
  indices_buffer = 0;
  indices_buffer_size = 0;
}
