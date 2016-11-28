#pragma once
#include "Timer.h"
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <random>
#include <unordered_map>
using namespace glm;
struct aiString;

#define MAX_INSTANCE_COUNT 100
#define UNIFORM_LIGHT_LOCATION 20
#define MAX_LIGHTS 10
#define SHOW_ERROR_TEXTURE 0
#define DYNAMIC_TEXTURE_RELOADING 1
#define DYNAMIC_FRAMERATE_TARGET 1
#define DEBUG 1
#define ENABLE_ASSERTS 1

extern std::mt19937 generator;
extern const float32 dt;
extern const float32 MOVE_SPEED;
extern const float32 MOUSE_X_SENS;
extern const float32 MOUSE_Y_SENS;
extern uint32 LAST_RENDER_ENTITY_COUNT;
extern const std::string BASE_TEXTURE_PATH;
extern const std::string BASE_SHADER_PATH;
extern const std::string BASE_MODEL_PATH;
extern const std::string ERROR_TEXTURE_PATH;
extern Timer PERF_TIMER;

// todo: make some variadic template for this crap:
bool all_equal(int32 a, int32 b, int32 c);
bool all_equal(int32 a, int32 b, int32 c, int32 d);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f, int32 g);
template <typename T> uint32 array_count(T t)
{
  return sizeof(t) / sizeof(t[0]);
}
float32 rand(float32 min, float32 max);
vec3 rand(vec3 max);
// used to fix double escaped or wrong-slash file paths
// probably need something better
std::string fix_filename(std::string str);
std::string copy(aiString str);
std::string read_file(const char *filePath);
template <typename T> void ASSERT(T t)
{
#ifndef DISABLE_ASSERT
  if (!t)
  {
    throw;
  }
#endif
}

#define check_gl_error() _check_gl_error(__FILE__, __LINE__)
void _check_gl_error(const char *file, uint32 line);

void checkSDLError(int32 line = -1);
uint32 new_ID();

// actual program time
// don't use for game simulation
float64 get_real_time();

// adds a message to a container that is retrieved by get_messages()
// subsequent calls to this function with identical identifiers will
// overwrite the older message
void set_message(std::string identifier, std::string message = "",
                 float64 msg_duration = 1.0);

// returns messages set with set_message()
// as string(identifier+" "+message)
// for all messages with unique identifier
std::string get_messages();