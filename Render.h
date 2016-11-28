#pragma once
#include "Globals.h"
#include "Mesh_Loader.h"
#include "Shader.h"
#include "stb_image.h"
#include <GL/glew.h>
#include <SDL.h>
#include <array>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace glm;

struct Texture
{
  struct Texture_Handle
  {
    ~Texture_Handle();
    GLuint texture = 0;
    time_t file_mod_t = 0;
  };
  Texture();
  Texture(std::string path);

private:
  friend struct Render;
  friend struct Material;
  void load();
  void bind(const char *name, GLuint location, Shader &shader);
  static std::unordered_map<std::string, std::weak_ptr<Texture_Handle>> cache;
  std::shared_ptr<Texture_Handle> texture;
  std::string file_path;
};

// todo: why did i have to regress? Mesh used to manage itself like Texture
// there was a reason but i forgot
struct Mesh
{
  Mesh();
  Mesh(std::string mesh_name);
  Mesh(Mesh_Data mesh_data, std::string mesh_name);
  Mesh(const aiMesh *aimesh);
  Mesh(Mesh &&rhs);
  Mesh(const Mesh &rhs) = delete;
  Mesh &operator=(const Mesh &rhs) = delete;
  Mesh &operator=(Mesh &&rhs) = delete;
  ~Mesh();

private:
  friend struct Render;
  void assign_instance_buffers(GLuint instance_MVP_Buffer,
                               GLuint instance_Model_Buffer, Shader &shader);
  void bind_to_shader(Shader &shader);
  bool instance_buffers_set = false;
  GLuint vao = 0;
  GLuint position_buffer = 0;
  GLuint normal_buffer = 0;
  GLuint uv_buffer = 0;
  GLuint tangents_buffer = 0;
  GLuint bitangents_buffer = 0;
  GLuint indices_buffer = 0;
  GLuint indices_buffer_size = 0;
  Mesh_Data data;
  std::string name;
  int skip_delete = 0;
  void upload_data(const Mesh_Data &mesh_data);
};

struct Material_Descriptor
{
  std::string albedo;
  std::string roughness;
  std::string specular;  // specular color for conductors   - unused for now
  std::string metalness; // boolean conductor or insulator - unused for now
  std::string tangent;   // anisotropic surface roughness    - unused for now
  std::string normal;
  std::string ambient_occlusion; // unused for now
  std::string emissive;
  std::string vertex_shader;
  std::string frag_shader;
};

struct Material
{
  Material();
  Material(Material_Descriptor m);
  Material(aiMaterial *ai_material, std::string working_directory,
           std::string vertex_shader, std::string fragment_shader);

private:
  friend struct Render;
  void load(Material_Descriptor m);
  void bind();
  void unbind_textures();
  Texture albedo;
  Texture specular_color;
  Texture normal;
  Texture emissive;
  Texture roughness;
  Shader shader;
  Material_Descriptor m;
};

enum Light_Type
{
  parallel,
  omnidirectional,
  spot
};

struct Light
{
  vec3 position = vec3(0, 0, 0);
  vec3 direction = vec3(0, 0, 0);
  vec3 color = vec3(1, 1, 1);
  vec3 attenuation = vec3(1, 0, 0);
  float ambient = 0.0f;
  float cone_angle = 1.0f;
  Light_Type type;
  bool operator==(const Light &rhs) const;
};

struct Light_Array
{
  bool operator==(const Light_Array &rhs);
  std::array<Light, MAX_LIGHTS> lights;
  uint32 count = 0;
};

// A render entity/render instance is a complete prepared representation of an
// object to be rendered by a draw call
// this should eventually contain the necessary skeletal animation data
struct Render_Entity
{
  Render_Entity(Mesh *mesh, Material *material, Light_Array lights,
                mat4 world_to_model);
  Light_Array lights;
  mat4 transformation;
  Mesh *mesh;
  Material *material;
  std::string name;
  uint32 ID;
};
// Similar to Render_Entity, but rendered with instancing
struct Render_Instance
{
  Render_Instance() {}
  Light_Array lights;
  std::vector<mat4> MVP_Matrices;
  std::vector<mat4> Model_Matrices;
  std::vector<uint32> IDs;
  Mesh *mesh;
  Material *material;
};
struct Render
{
  Render(SDL_Window *window, ivec2 window_size);
  ~Render();
  void render(float64 state_time);
  std::vector<Render_Instance> render_instances;
  bool use_txaa = false;
  void resize_window(ivec2 window_size);
  float32 get_render_scale()const { return render_scale; }
  float32 get_vfov() { return vfov; }
  void set_render_scale(float32 scale);
  void set_camera(vec3 camera_position, vec3 camera_gaze);
  void set_vfov(float32 vfov); // vertical field of view in degrees
  SDL_Window *window;
  void set_render_entities(std::vector<Render_Entity> entities);
  float64 target_frame_time = 1.0 / 60.0;
  uint64 frame_count = 0;

private:
  float64 time_of_last_scale_change = 0.;
  Timer frame_timer = Timer(60);
  Timer swap_timer = Timer(60);
  void init_render_targets();
  void dynamic_framerate_target();
  bool prev_color_target_missing = true;
  std::vector<Render_Entity> previous_render_entities;
  mat4 get_next_TXAA_sample();
  float32 render_scale = 2.0f; // supersampling
  ivec2 window_size;           // actual window size
  ivec2 size;                  // render target size
  float32 vfov = 60;
  mat4 camera;
  mat4 projection;
  vec3 camera_position = vec3(0);
  vec3 prev_camera_position = vec3(0);
  bool jitter_switch = false;
  mat4 txaa_jitter = mat4(1);
  GLuint target_fbo = 0;
  GLuint color_target = 0;
  GLuint depth_target = 0;
  GLuint prev_color_target = 0;
  GLuint instance_MVP_buffer = 0;
  GLuint instance_Model_buffer = 0;
  void check_and_clear_expired_textures();
  Mesh quad = Mesh("plane");
  Shader temporalAA = Shader("passthrough.vert", "TemporalAA.frag");
  Shader passthrough = Shader("passthrough.vert", "passthrough.frag");
};
