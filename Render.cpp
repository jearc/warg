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

// our attachment points for deferred rendering
#define DIFFUSE_TARGET GL_COLOR_ATTACHMENT0
#define NORMAL_TARGET GL_COLOR_ATTACHMENT1
#define POSITION_TARGET GL_COLOR_ATTACHMENT2
#define DEPTH_TARGET GL_COLOR_ATTACHMENT3

const GLenum RENDER_TARGETS[] = {DIFFUSE_TARGET, NORMAL_TARGET, POSITION_TARGET,
                                 DEPTH_TARGET};
#define TARGET_COUNT sizeof(RENDER_TARGETS) / sizeof(GLenum)

enum Texture_Location
{
  albedo,
  specular,
  normal,
  emissive,
  roughness
};
std::unordered_map<std::string, std::weak_ptr<Texture::Texture_Handle>>
    Texture::cache;
Texture::Texture() { file_path = ERROR_TEXTURE_PATH; }
Texture::Texture_Handle::~Texture_Handle()
{
  glDeleteTextures(1, &texture);
  texture = 0;
}
Texture::Texture(std::string path)
{
  // the reason for this is that sometimes
  // the assimp asset you load will have screwy paths
  // depending on the program that exported it
  path = fix_filename(path);
  if (path.size() == 0)
    path = ERROR_TEXTURE_PATH;

  size_t i = path.find_last_of("/");
  if (i == path.npos)
  { // no specified directory, so use base path
    file_path = BASE_TEXTURE_PATH + path;
  }
  else
  { // assimp imported model or user specified a directory
    file_path = path;
  }

  // force rename to png
  size_t end = file_path.size();
  ASSERT(end > 3);
  if (file_path[end - 1] != 'g' || file_path[end - 2] != 'n' ||
      file_path[end - 3] != 'p')
  {
    file_path[end - 1] = 'g';
    file_path[end - 2] = 'n';
    file_path[end - 3] = 'p';

    set_message("Warning, specified texture path invalid filetype. Renaming to: ",file_path+"\n",1.0);
  }
}

void Texture::load()
{
#if !SHOW_ERROR_TEXTURE
  if (file_path == ERROR_TEXTURE_PATH || file_path == BASE_TEXTURE_PATH)
  { // a null texture bound and rendered will show as (0,0,0,0) in the shader
    texture = nullptr;
    return;
  }
#endif
  auto ptr = Texture::cache[file_path].lock();
  if (ptr)
  {
    texture = ptr;
    return;
  }
  texture = std::make_shared<Texture_Handle>();
  cache[file_path] = texture;
  int32 width, height, n;
  auto *data = stbi_load(file_path.c_str(), &width, &height, &n, 4);
  if (!data)
  {
#if DYNAMIC_TEXTURE_RELOADING
    // retry next frame
    set_message("Warning: missing texture:" + file_path);
    texture = nullptr;
    return;
#else
    if (!data)
    {
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
Mesh::Mesh(std::string mesh_name) : name(mesh_name)
{
  data = load_mesh(name.c_str());
  upload_data(data);
}
Mesh::Mesh(Mesh_Data mesh_data, std::string mesh_name)
    : data(mesh_data), name(mesh_name)
{
  data = mesh_data;
  upload_data(data);
}
Mesh::Mesh(const aiMesh *aimesh)
{
  ASSERT(aimesh);
  data = load_mesh(aimesh);
  upload_data(data);
}
Mesh::Mesh(Mesh &&rhs)
{
  vao = rhs.vao;
  position_buffer = rhs.position_buffer;
  normal_buffer = rhs.normal_buffer;
  uv_buffer = rhs.uv_buffer;
  tangents_buffer = rhs.tangents_buffer;
  bitangents_buffer = rhs.bitangents_buffer;
  indices_buffer = rhs.indices_buffer;
  indices_buffer_size = rhs.indices_buffer_size;
  data = std::move(rhs.data);
  name = rhs.name;

  rhs.skip_delete = 1;
}

Mesh::~Mesh()
{
  if (!skip_delete)
  {
    glDeleteBuffers(1, &position_buffer);
    glDeleteBuffers(1, &normal_buffer);
    glDeleteBuffers(1, &uv_buffer);
    glDeleteBuffers(1, &tangents_buffer);
    glDeleteBuffers(1, &bitangents_buffer);
    glDeleteBuffers(1, &indices_buffer);
    glDeleteVertexArrays(1, &vao);
  }
}

void Mesh::assign_instance_buffers(GLuint instance_MVP_Buffer,
                                   GLuint instance_Model_Buffer, Shader &shader)
{

  if (!instance_buffers_set)
  {
    instance_buffers_set = true;
    GLint current_vao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &current_vao);
    ASSERT(current_vao == vao);
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
  int32 loc = glGetAttribLocation(shader.program->program, "position");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(loc);
  }
  loc = glGetAttribLocation(shader.program->program, "normal");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(loc);
  }
  loc = glGetAttribLocation(shader.program->program, "uv");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
    glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(float32) * 2, 0);
    glEnableVertexAttribArray(loc);
  }
  loc = glGetAttribLocation(shader.program->program, "tangent");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, tangents_buffer);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(loc);
  }
  loc = glGetAttribLocation(shader.program->program, "bitangent");
  if (loc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, bitangents_buffer);
    glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, sizeof(float32) * 3, 0);
    glEnableVertexAttribArray(loc);
  }
}

void Mesh::upload_data(const Mesh_Data &mesh_data)
{
  if (sizeof(decltype(mesh_data.indices)::value_type) != sizeof(uint32))
  {
    set_message("Mesh::upload_data error: render() assumes index type to be 32 bits");
    ASSERT(0);
  }

  check_gl_error();
  indices_buffer_size = mesh_data.indices.size();

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &position_buffer);
  glGenBuffers(1, &normal_buffer);
  glGenBuffers(1, &indices_buffer);
  glGenBuffers(1, &uv_buffer);
  glGenBuffers(1, &tangents_buffer);
  glGenBuffers(1, &bitangents_buffer);

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
  glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.positions[0],
               GL_STATIC_DRAW);

  // normals
  buffer_size = mesh_data.normals.size() *
                sizeof(decltype(mesh_data.normals)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.normals[0],
               GL_STATIC_DRAW);

  // texture_coordinates
  buffer_size = mesh_data.texture_coordinates.size() *
                sizeof(decltype(mesh_data.texture_coordinates)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.texture_coordinates[0],
               GL_STATIC_DRAW);

  // indices
  buffer_size = mesh_data.indices.size() *
                sizeof(decltype(mesh_data.indices)::value_type);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, buffer_size, &mesh_data.indices[0],
               GL_STATIC_DRAW);

  // tangents
  buffer_size = mesh_data.tangents.size() *
                sizeof(decltype(mesh_data.tangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, tangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.tangents[0],
               GL_STATIC_DRAW);

  // bitangents
  buffer_size = mesh_data.bitangents.size() *
                sizeof(decltype(mesh_data.bitangents)::value_type);
  glBindBuffer(GL_ARRAY_BUFFER, bitangents_buffer);
  glBufferData(GL_ARRAY_BUFFER, buffer_size, &mesh_data.bitangents[0],
               GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  name = mesh_data.name;
}

Material::Material() {}
Material::Material(Material_Descriptor m) { load(m); }
Material::Material(aiMaterial *ai_material, std::string working_directory,
                   std::string vertex_shader, std::string fragment_shader)
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
  m.albedo = copy(name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_SPECULAR, 0, &name);
  m.specular = copy(name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_EMISSIVE, 0, &name);
  m.emissive = copy(name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_NORMALS, 0, &name);
  m.normal = copy(name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_LIGHTMAP, 0, &name);
  m.ambient_occlusion = copy(name);
  name.Clear();
  ai_material->GetTexture(aiTextureType_SHININESS, 0, &name);
  m.roughness = copy(name);
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

  m.vertex_shader = vertex_shader;
  m.frag_shader = fragment_shader;
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
}
void Material::bind()
{
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
  if (count != rhs.count)
    return false;
  if (lights != rhs.lights)
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

Render::~Render()
{
  glDeleteFramebuffers(1, &target_fbo);
  glDeleteTextures(1, &color_target);
  glDeleteTextures(1, &prev_color_target);
  glDeleteRenderbuffers(1, &depth_target);
  glDeleteBuffers(1, &instance_MVP_buffer);
  glDeleteBuffers(1, &instance_Model_buffer);
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

  // instancing MVP Matrix buffer init
  const GLuint mat4_size = sizeof(GLfloat) * 4 * 4;
  const GLuint instance_buffer_size = MAX_INSTANCE_COUNT * mat4_size;

  glGenBuffers(1, &instance_MVP_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_MVP_buffer);
  glBufferData(GL_ARRAY_BUFFER, instance_buffer_size, (void *)0,
               GL_DYNAMIC_DRAW);

  // instancing Model Matrix buffer init
  glGenBuffers(1, &instance_Model_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, instance_Model_buffer);
  glBufferData(GL_ARRAY_BUFFER, instance_buffer_size, (void *)0,
               GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  frame_timer.start();
}

void Render::check_and_clear_expired_textures()
{
  auto it = Texture::cache.begin();
  while (it != Texture::cache.end())
  {
    auto texture_handle = *it;
    const std::string &path = texture_handle.first;
    struct stat attr;
    stat(path.c_str(), &attr);
    time_t t = attr.st_mtime;

    auto ptr = texture_handle.second.lock();
    if (ptr) // texture could exist but not yet be loaded
    {
      time_t t1 = ptr.get()->file_mod_t;
      if (t1 != t)
      {
        it = Texture::cache.erase(it);
        continue;
      }
    }
    ++it;
  }
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
  shader.set_uniform("number_of_lights", (int32)lights.count);
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
  glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
  glFramebufferTexture(GL_FRAMEBUFFER, DIFFUSE_TARGET, color_target, 0);
  vec3 night = vec3(0.05f);
  vec3 day = vec3(94. / 255., 155. / 255., 1.);
  float32 t1 = sin(time / 3);
  t1 = clamp(t1, -1.0f, 1.0f);
  t1 = (t1 / 2.0f) + 0.5f;
  vec3 color = lerp(night, day, t1);

  glClearColor(day.r, day.g, day.b, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  for (auto &entity : render_instances)
  {
    glBindVertexArray(entity.mesh->vao);
    Shader &shader = entity.material->shader;
    shader.use();
    entity.material->bind();
    entity.mesh->bind_to_shader(shader);
    shader.set_uniform("time", time);
    shader.set_uniform("txaa_jitter", txaa_jitter);
    shader.set_uniform("camera_position", camera_position);
    if (entity.mesh->name == "plane")
    { // uv scale just for the ground - bad obv
      shader.set_uniform("uv_scale", vec2(8));
    }
    else
    {
      shader.set_uniform("uv_scale", vec2(1));
    }
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

    entity.mesh->assign_instance_buffers(instance_MVP_buffer,
                                         instance_Model_buffer, shader);

    glBindBuffer(GL_ARRAY_BUFFER, instance_MVP_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size,
                    &entity.MVP_Matrices[0][0][0]);
    glBindBuffer(GL_ARRAY_BUFFER, instance_Model_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buffer_size,
                    &entity.Model_Matrices[0][0][0]);


    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entity.mesh->indices_buffer);
    glDrawElementsInstanced(GL_TRIANGLES, entity.mesh->indices_buffer_size,
                            GL_UNSIGNED_INT, (void *)0, num_instances);

    entity.material->unbind_textures();
  }
  check_gl_error();
  glBindVertexArray(quad.vao);

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

    temporalAA.use();
    quad.bind_to_shader(temporalAA);
    GLuint u = glGetUniformLocation(temporalAA.program->program, "current");
    glUniform1i(u, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_target);
    GLuint u2 = glGetUniformLocation(temporalAA.program->program, "previous");
    glUniform1i(u2, 1);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D,
                  prev_color_target_missing ? color_target : prev_color_target);
    temporalAA.set_uniform("transform", o);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.indices_buffer);
    glDrawElements(GL_TRIANGLES, quad.indices_buffer_size, GL_UNSIGNED_INT,
                   (void *)0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFinish(); // intent is to time just the swap itself
    frame_timer.stop();
    swap_timer.start();
    SDL_GL_SwapWindow(window);
    glFinish();
    swap_timer.stop();
    frame_timer.start();

    // copy this frame to previous framebuffer
    // so it will be usable next frame, when its the old frame

    // assign previous_color as the target to overwrite
    glViewport(0, 0, size.x, size.y);
    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         prev_color_target, 0);
    passthrough.use();
    quad.bind_to_shader(passthrough);
    // sample the current color_target as source
    GLuint u3 = glGetUniformLocation(passthrough.program->program, "albedo");
    glUniform1i(u3, Texture_Location::albedo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_target);
    passthrough.set_uniform("transform", o);
    glDrawElements(GL_TRIANGLES, quad.indices_buffer_size, GL_UNSIGNED_INT,
                   (void *)0);

    prev_color_target_missing = false;
    // place color_target back in target_fbo
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_target, 0);
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
    passthrough.use();
    quad.bind_to_shader(passthrough);
    GLuint u = glGetUniformLocation(passthrough.program->program, "albedo");
    glUniform1i(u, Texture_Location::albedo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_target);
    passthrough.set_uniform("transform", o);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.indices_buffer);
    glDrawElements(GL_TRIANGLES, quad.indices_buffer_size, GL_UNSIGNED_INT,
                   (void *)0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glFinish(); // intent is to time just the swap itself
    frame_timer.stop();
    swap_timer.start();
    SDL_GL_SwapWindow(window);
    glFinish();
    swap_timer.stop();
    frame_timer.start();
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

void Render::set_camera(vec3 camera_position, vec3 camera_gaze_dir)
{
  vec3 camera_gaze_point = camera_position + camera_gaze_dir;
  this->camera_position = camera_position;
  camera = glm::lookAt(camera_position, camera_gaze_point, {0, 0, 1});
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
        ASSERT(instance->lights.count == entity->lights.count);
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
  glDeleteFramebuffers(1, &target_fbo);
  glDeleteTextures(1, &color_target);
  glDeleteTextures(1, &prev_color_target);
  glDeleteRenderbuffers(1, &depth_target);

  size = ivec2(render_scale * window_size.x, render_scale * window_size.y);
  glGenFramebuffers(1, &target_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);

  glGenTextures(1, &color_target);
  glBindTexture(GL_TEXTURE_2D, color_target);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  glGenTextures(1, &prev_color_target);
  glBindTexture(GL_TEXTURE_2D, prev_color_target);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  glFramebufferTexture(GL_FRAMEBUFFER, DIFFUSE_TARGET, color_target, 0);
  glDrawBuffers(TARGET_COUNT, RENDER_TARGETS);
  glGenRenderbuffers(1, &depth_target);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_target);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size.x, size.y);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth_target);

  prev_color_target_missing = true;

  set_message("Init render targets: ", std::to_string(target_fbo) + " " +
    std::to_string(prev_color_target) + " " +
    std::to_string(depth_target));
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
  const float32 increase_factor = 1.05f;
  // gives a little extra time before swap+frame > 1/hz counts as a missed vsync
  // not entirely sure why this is necessary, but at 0 swap+frame is almost
  // always > 1/hz
  // driver overhead and/or timer overhead inaccuracies
  const float64 epsilon = 0.005;
  // if swap alone took longer than this, we should increase the resolution
  // because we probably had a lot of extra gpu time because this blocked for so
  // long
  // not sure if there's a better way to find idle gpu use
  const float64 longest_swap_allowable = target_frame_time * .45;

  if (frame_timer.sample_count() < min_samples ||
      swap_timer.sample_count() < min_samples)
  { // don't adjust until we have good avg frame time data
    return;
  }
  // ALL time other than swap time
  const float64 moving_avg = frame_timer.moving_average();
  const float64 last_frame = frame_timer.get_last();

  // ONLY swap time
  const float64 last_swap = swap_timer.get_last();
  const float64 swap_avg = swap_timer.moving_average();

  auto frame_times = frame_timer.get_times();
  auto swap_times = swap_timer.get_times();
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
    uint64 last_start = frame_timer.get_begin();
    swap_timer.clear_all();
    frame_timer.clear_all();
    frame_timer.start(last_start);
  }
  else if (swap_avg > longest_swap_allowable)
  {
    if (render_scale != 2.0) // max scale 2.0
    {
      float64 change_rate = .55;
      if (get_real_time() < time_of_last_scale_change + change_rate)
      { // increase resolution slowly
        return;
      }
      set_message("High avg swap. Increasing resolution.",std::to_string(swap_avg)+" "+std::to_string(target_frame_time));
      set_render_scale(render_scale * increase_factor);
      uint64 last_start = frame_timer.get_begin();
      swap_timer.clear_all();
      frame_timer.clear_all();
      frame_timer.start(last_start);
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
