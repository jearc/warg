#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
using namespace glm;
struct Shader
{
  Shader();
  Shader(const std::string &vertex, const std::string &fragment);
  void load(const std::string &vertex, const std::string &fragment);

  void set_uniform(const char *name, uint32 i);
  void set_uniform(const char *name, float32 f);
  void set_uniform(const char *name, vec2 v);
  void set_uniform(const char *name, vec3 &v);
  void set_uniform(const char *name, vec4 &v);
  void set_uniform(const char *name, const mat4 &m);

  void use() const;

  struct Shader_Handle
  {
    Shader_Handle(GLuint i);
    ~Shader_Handle();
    GLuint program = 0;
  };
  std::shared_ptr<Shader_Handle> program;
  std::string name;

private:
  void check_err(GLint loc, const char *name);

  // todo: create create every uniform as an int =-1 and assign their binding
  // locations to avoid get_uniform_location spam
};