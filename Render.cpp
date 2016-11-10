#pragma once
#include <GL\glew.h>
#include <SDL2\SDL.h>
#include <assimp\Importer.hpp>
#include <assimp\postprocess.h>
#include <assimp\scene.h>
#include <assimp\types.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "Globals.h"
#include "Mesh_Loader.h"
#include "Render.h"
#include "Shader.h"
#include "Timer.h"
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\quaternion.hpp>
#include <glm\gtx\euler_angles.hpp>
#include <glm\gtx\transform.hpp>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
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

Texture::Texture()
{
  static std::shared_ptr<Texture> err =
      std::make_shared<Texture>(std::string("err.png"));
  filename = "err.png";
  load(BASE_TEXTURE_PATH + filename);
}
Texture::Texture_Handle::~Texture_Handle() { glDeleteTextures(1, &texture); }
Texture::Texture(const std::string &path)
{
  // the reason for this is that sometimes
  // the assimp asset you load will have screwy paths
  // depending on the program that exported it
  filename = fix_filename(path);
  if (filename.size() == 0)
  {
    filename = "err.png";

    if (!SHOW_ERROR_TEXTURE)
    {
      return;
    }
  }
  load(BASE_TEXTURE_PATH + filename);
}
void Texture::load(const std::string &path)
{
  static std::unordered_map<std::string, std::weak_ptr<Texture_Handle>> cache;

  auto ptr = cache[path].lock();
  if (ptr)
  {
    texture = ptr;
    // std::cout << "Texture load cache hit: " << path << "\n";
    return;
  }
  texture = std::make_shared<Texture_Handle>();
  cache[path] = texture;
  int32 width, height, n;
  auto *data = stbi_load(path.c_str(), &width, &height, &n, 4);

  ASSERT(data);
  std::cout << "Texture load cache miss. Texture from disk: " << path << "\n";

  // TODO: optimize the texture storage types to save GPU memory
  // currently  everything is stored with RGBA
  // could pack maps together like: RGBA with diffuse as RGB and A as
  // ambient_occlusion

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

void Texture::bind(const char *name, GLuint binding, Shader &shader) const
{
  if (texture)
  {
    GLuint u = glGetUniformLocation(shader.program->program, name);
    glUniform1i(u, binding);
    glActiveTexture(GL_TEXTURE0 + (GLuint)binding);
    glBindTexture(GL_TEXTURE_2D, texture->texture);
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

    check_gl_error();
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
      check_gl_error();
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
      check_gl_error();
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
    throw "render() assumes index type to be 32 bits";
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
  ai_material->GetTexture(aiTextureType_SPECULAR, 0, &name);
  m.specular = copy(name);
  ai_material->GetTexture(aiTextureType_EMISSIVE, 0, &name);
  m.emissive = copy(name);
  ai_material->GetTexture(aiTextureType_NORMALS, 0, &name);
  m.normal = copy(name);
  ai_material->GetTexture(aiTextureType_LIGHTMAP, 0, &name);
  m.ambient_occlusion = copy(name);
  ai_material->GetTexture(aiTextureType_SHININESS, 0, &name);
  m.roughness = copy(name);

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
  // TODO cleanup framebuffers
}

Render::Render(SDL_Window *window, ivec2 window_size)
{
  this->window = window;
  this->window_size = window_size;
  size = ivec2(render_scale * window_size.x, render_scale * window_size.y);
  set_vfov(vfov);
  check_gl_error();
  glGenFramebuffers(1, &target_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);

  check_gl_error();
  glGenTextures(1, &color_target);
  glBindTexture(GL_TEXTURE_2D, color_target);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  check_gl_error();
  glGenTextures(1, &prev_color_target);
  glBindTexture(GL_TEXTURE_2D, prev_color_target);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0);

  check_gl_error();
  glFramebufferTexture(GL_FRAMEBUFFER, DIFFUSE_TARGET, color_target, 0);
  glDrawBuffers(TARGET_COUNT, RENDER_TARGETS);
  glGenRenderbuffers(1, &depth_target);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_target);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, size.x, size.y);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth_target);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
  check_gl_error();
}

void Render::render(float64 t, float64 time)
{
  check_gl_error();
  glViewport(0, 0, size.x, size.y);
  glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
  glFramebufferTexture(GL_FRAMEBUFFER, DIFFUSE_TARGET, color_target, 0);
  vec3 night = vec3(0.05f);
  vec3 day = vec3(94. / 255., 155. / 255., 1.);
  float32 t1 = 5 * sin(float32(time) / 3);
  t1 = clamp(t1, -1.0f, 1.0f);
  t1 = (t1 / 2.0f) + 0.5f;
  vec3 color = lerp(night, day, t1);

  glClearColor(color.r, color.g, color.b, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  for (auto &entity : render_instances)
  {
    glBindVertexArray(entity.mesh->vao);
    Shader *shader = &entity.material->shader;
    ASSERT(shader);
    shader->use();
    entity.material->bind();
    entity.mesh->bind_to_shader(*shader);
    shader->set_uniform("time", float32(time));
    shader->set_uniform("txaa_jitter", txaa_jitter);
    shader->set_uniform("camera_position", camera_position);
    if (entity.mesh->name == "plane")
    { // uv scale just for the ground - bad obv
      shader->set_uniform("uv_scale", vec2(8));
    }
    else
    {
      shader->set_uniform("uv_scale", vec2(1));
    }

    ASSERT(entity.lights.lights.size() == MAX_LIGHTS);
    uint32 location = -1;
// todo: this is horrible. do something much better than this - precompute
// all these godawful strings and just select them
#define s std::string
#define ts std::to_string

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
      shader->set_uniform((s("lights[") + ts(i) + s("].position")).c_str(),
                          entity.lights.lights[i].position);
      shader->set_uniform((s("lights[") + ts(i) + s("].direction")).c_str(),
                          entity.lights.lights[i].direction);
      shader->set_uniform((s("lights[") + ts(i) + s("].color")).c_str(),
                          entity.lights.lights[i].color);
      shader->set_uniform((s("lights[") + ts(i) + s("].attenuation")).c_str(),
                          entity.lights.lights[i].attenuation);
      vec3 ambient =
          entity.lights.lights[i].ambient * entity.lights.lights[i].color;

      shader->set_uniform((s("lights[") + ts(i) + s("].ambient")).c_str(),
                          ambient);
      shader->set_uniform((s("lights[") + ts(i) + s("].cone_angle")).c_str(),
                          entity.lights.lights[i].cone_angle);
      shader->set_uniform((s("lights[") + ts(i) + s("].type")).c_str(),
                          (uint32)entity.lights.lights[i].type);
    }
    shader->set_uniform("number_of_lights", entity.lights.count);

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
                                         instance_Model_buffer, *shader);

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
  static Shader temporalAA("passthrough.vert", "TemporalAA.frag");
  static Shader passthrough("passthrough.vert", "passthrough.frag");
  static Mesh quad("plane");
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
    glBindTexture(GL_TEXTURE_2D, prev_color_target);
    temporalAA.set_uniform("transform", o);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.indices_buffer);
    glDrawElements(GL_TRIANGLES, quad.indices_buffer_size, GL_UNSIGNED_INT,
                   (void *)0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    SDL_GL_SwapWindow(window);

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
    SDL_GL_SwapWindow(window);
  }
}

void Render::resize_window(ivec2 window_size)
{
  this->window_size = window_size;
  set_vfov(vfov);
  ASSERT(0); // not yet implemented
}

void Render::set_render_scale(float32 scale)
{
  render_scale = scale;
  ASSERT(0); // reset FBOs not yet implemented
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
  // lerp(current, current + (current-prev), t) in renderer
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

mat4 Render::get_next_TXAA_sample()
{
  mat4 result;
  vec2 translation;
  if (jitter_switch)
    translation = vec2(0.5, 0.5);
  else
    translation = vec2(-0.5, -0.5);

  result = glm::translate(vec3(translation / vec2(size), 0));
  jitter_switch = !jitter_switch;
  return result;
}
