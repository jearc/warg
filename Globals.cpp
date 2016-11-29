#include "Globals.h"
#include <GL/glew.h>
#include <SDL.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
using namespace glm;
std::mt19937 generator;
const float32 dt = 1.0f / 150.0f;
const float32 MOVE_SPEED = 2.0f * dt;
const float32 MOUSE_X_SENS = .0041f;
const float32 MOUSE_Y_SENS = .0041f;
const float32 ZOOM_STEP = 0.2f;
#ifdef __linux__
#define ROOT_PATH std::string("../")
#elif _WIN32
#define ROOT_PATH std::string("../")
#endif
const std::string BASE_TEXTURE_PATH =
    ROOT_PATH + std::string("Assets/Textures/");
const std::string BASE_SHADER_PATH = ROOT_PATH + std::string("Assets/Shaders/");
const std::string BASE_MODEL_PATH = ROOT_PATH + std::string("Assets/Models/");
const std::string ERROR_TEXTURE_PATH = BASE_TEXTURE_PATH + "err.png";
Timer PERF_TIMER = Timer(1000);

bool all_equal(int32 a, int32 b, int32 c) { return (a == b) && (a == c); }
bool all_equal(int32 a, int32 b, int32 c, int32 d)
{
  return (a == b) && (a == c) && (a == d);
}
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f)
{
  return (a == b) && (a == c) && (a == d) && (a == f);
}
bool all_equal(int32 a, int32 b, int32 c, int32 d, int32 f, int32 g)
{
  return (a == b) && (a == c) && (a == d) && (a == f) && (a == g);
}
float32 rand(float32 min, float32 max)
{
  std::uniform_real_distribution<float32> dist(min, max);
  float32 result = dist(generator);
  return result;
}
vec3 rand(vec3 max)
{
  return vec3(rand(0, max.x), rand(0, max.y), rand(0, max.z));
}

// used to fix double escaped or wrong-slash file paths that assimp sometimes
// gives
// todo: UTF8 support for all filenames
std::string fix_filename(std::string str)
{
  std::string result;
  uint32 size = str.size();
  for (uint32 i = 0; i < size; ++i)
  {
    char c = str[i];
    if (c == '\\')
    { // replace a backslash with a forward slash
      uint32 i2 = i + 1;
      if (i2 < size)
      {
        char c2 = str[i2];
        if (c2 == '\\')
        { // skip the second backslash if exists
          ++i;
          continue;
        }
      }
      result.push_back('/');
      continue;
    }
    result.push_back(c);
  }
  return result;
}

std::string copy(aiString str)
{
  std::string result;
  result.resize(str.length);
  result = str.C_Str();
  return result;
}

std::string read_file(const char *filePath)
{
  std::string content;
  std::ifstream fileStream(filePath, std::ios::in);
  if (!fileStream.is_open())
  {
    std::cerr << "Could not read file " << filePath << ". File does not exist."
              << std::endl;
    return "";
  }
  std::string line = "";
  while (!fileStream.eof())
  {
    std::getline(fileStream, line);
    content.append(line + "\n");
  }
  fileStream.close();
  return content;
}

void _check_gl_error(const char *file, uint32 line)
{
  glFlush();
  GLenum err(glGetError());
  glFlush();
  while (err != GL_NO_ERROR)
  {
    std::string error;
    switch (err)
    {
      case GL_INVALID_OPERATION:
        error = "INVALID_OPERATION";
        break;
      case GL_INVALID_ENUM:
        error = "INVALID_ENUM";
        break;
      case GL_INVALID_VALUE:
        error = "INVALID_VALUE";
        break;
      case GL_OUT_OF_MEMORY:
        error = "OUT_OF_MEMORY";
        break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        error = "INVALID_FRAMEBUFFER_OPERATION";
        break;
    }
    std::cerr << "GL_" << error.c_str() << " - " << file << ":" << line
              << std::endl;
    err = glGetError();
    ASSERT(0);
  }
}
void checkSDLError(int32 line)
{
#ifdef DEBUG
  const char *error = SDL_GetError();
  if (*error != '\0')
  {
    printf("SDL Error: %s\n", error);
    if (line != -1)
      printf(" + line: %i\n", line);
    SDL_ClearError();
  }
#endif
}
uint32 new_ID()
{
  static uint32 last = 0;
  return last += 1;
}

float64 get_real_time()
{
  static const Uint64 freq = SDL_GetPerformanceFrequency();
  static const Uint64 begin_time = SDL_GetPerformanceCounter();

  Uint64 current = SDL_GetPerformanceCounter();
  Uint64 elapsed = current - begin_time;
  return (float64)elapsed / (float64)freq;
}
static struct Message
{
  std::string message;
  float64 time_of_expiry;
};
static std::unordered_map<std::string, Message> messages;
static std::string message_log = "";
void set_message(std::string identifier, std::string message,
                 float64 msg_duration)
{
  const float64 time = get_real_time();
  Message m;
  m.message = message;
  m.time_of_expiry = time + msg_duration;
  messages[identifier] = m;
  message_log.append("time: " + std::to_string(time) + "--- " + identifier + " " + message + "\n\n");
}
std::string get_messages()
{
  std::string result;
  float64 time = get_real_time();
  auto it = messages.begin();
  while (it != messages.end())
  {
    if (it->second.time_of_expiry < time)
    {
      it = messages.erase(it);
      continue;
    }
    result = result + it->first + std::string(" ") + it->second.message +
             std::string("\n");
    ++it;
  }
  return result;
}
void push_log_to_disk()
{
  static bool first = true;
  if (first)
  {
    std::fstream file("warg_log.txt", std::ios::trunc);
    first = false;
  }
  std::fstream file("warg_log.txt", std::ios::in | std::ios::out);
  file.seekg(std::ios::end);
  file.write(message_log.c_str(), message_log.size());
  message_log.clear();
}