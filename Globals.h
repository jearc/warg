#pragma once
#include "Timer.h"
#include <fstream>
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <iostream>
#include <random>
#include <unordered_map>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
using namespace glm;
struct aiString;

#define MAX_INSTANCE_COUNT 100
#define UNIFORM_LIGHT_LOCATION 20
#define MAX_LIGHTS 10
#define SHOW_ERROR_TEXTURE 0
#define DYNAMIC_TEXTURE_RELOADING 1
#define DYNAMIC_FRAMERATE_TARGET 0
#define DEBUG 1
#define ENABLE_ASSERTS 1 

struct Game_State;
struct Render_Test_State;


extern std::mt19937 generator;
extern const float32 dt;
extern const float32 MOVE_SPEED;
extern const float32 ATK_RANGE;
extern const float32 MOUSE_X_SENS;
extern const float32 MOUSE_Y_SENS;
extern const float32 ZOOM_STEP;
extern uint32 LAST_RENDER_ENTITY_COUNT;

extern const std::string BASE_ASSET_PATH;
extern const std::string BASE_TEXTURE_PATH;
extern const std::string BASE_SHADER_PATH;
extern const std::string BASE_MODEL_PATH;
extern const std::string ERROR_TEXTURE_PATH;
extern Timer PERF_TIMER;


// load an aiScene
// will cache the result so that future loads
// of the same path won't have to read from disk
extern const aiScene* load_aiscene(std::string path, const int* assimp_flags=nullptr);


// todo: make some variadic template for this crap:
bool all_equal(int32 a, int32 b, int32 c);
bool all_equal(int32 a, int32 b, int32 c, int32 d);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f);
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f, int32 g); 

float32 wrap_to_range(const float32 input, const float32 min, const float32 max);
template <typename T> uint32 array_count(T t)
{
  return sizeof(t) / sizeof(t[0]);
}
float32 rand(float32 min, float32 max);
vec3 rand(vec3 max);
// used to fix double escaped or wrong-slash file paths
// probably need something better
std::string fix_filename(std::string str);
std::string copy(const aiString* str);
std::string read_file(const char *path);

#define ASSERT(x) _errr(x,__FILE__,__LINE__)

Uint32 string_to_color(std::string color);
Uint64 hash(float* data, uint32 size);



#define check_gl_error() _check_gl_error(__FILE__, __LINE__)
void _check_gl_error(const char *file, uint32 line);

void checkSDLError(int32 line = -1);
uint32 new_ID();

// actual program time
// don't use for game simulation
float64 get_real_time();

// adds a message to a container that is retrieved by get_messages()
// subsequent calls to this function with identical identifiers will
// overwrite the older message when retrieved with get_message_log()
// but ALL messages will be logged to disk with push_log_to_disk()
// a duration of 0 will not show up in the console, but will be logged
void set_message(std::string identifier, std::string message = "",
                 float64 msg_duration = 0.0);

// returns messages set with set_message()
// as string(identifier+" "+message)
// for all messages with unique identifier
std::string get_messages();

//appends all messages to disk that have been set
//since the last call to this function
void push_log_to_disk();


template <typename T> void _errr(T t,const char* file, uint32 line)
{
#ifndef DISABLE_ASSERT
  if (!t)
  {
    set_message("", "\nAssertion failed in:" + std::string(file) +"\non line:" + std::to_string(line),1.0);
    std::cout << get_messages();
    push_log_to_disk();
    throw;
  }
#endif
}