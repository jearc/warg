#include "Globals.h"
#include "Render.h"
#include "State.h"
#include "Timer.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <stdlib.h>

static double get_time()
{
  static const Uint64 freq = SDL_GetPerformanceFrequency();
  static const Uint64 begin_time = SDL_GetPerformanceCounter();

  Uint64 current = SDL_GetPerformanceCounter();
  Uint64 elapsed = current - begin_time;
  return (float64)elapsed / (float64)freq;
}

static void performance_output(const State *state, float64 current_time,
                               Uint64 frame_count)
{
  static float64 last_output = 0;
  static Uint64 frames_at_last_tick = 0;
  const float64 delay = 0.1;
  if (last_output + delay < current_time)
  {
#ifdef __linux__
    system("clear");
#elif _WIN32
    system("cls");
#endif
    std::cout << PERF_TIMER.string_report();

    Uint64 frames_this_sec =
        Uint64(round((1.0 / delay) * (frame_count - frames_at_last_tick)));
    std::cout << "FPS: " << frames_this_sec << "\n";
    std::cout << "Total FPS:" << frame_count / current_time << "\n";
    frames_at_last_tick = frame_count;
    last_output = current_time;
    std::cout << "Draw calls: " << state->renderer.render_instances.size()
              << "\n";
  }
}

int main(int argc, char *argv[])
{
  SDL_ClearError();
  generator.seed(1234);
  SDL_Init(SDL_INIT_EVERYTHING);
  uint32 display_count = uint32(SDL_GetNumVideoDisplays());
  for (uint32 i = 0; i < display_count; ++i)
  {
    std::cout << "Display " << i << ":\n";
    SDL_DisplayMode mode;
    uint32 mode_count = uint32(SDL_GetNumDisplayModes(i));
    for (uint32 j = 0; j < mode_count; ++j)
    {
      SDL_GetDisplayMode(i, j, &mode);
      std::cout << "Supported resolution: " << mode.w << "x" << mode.h << " "
                << mode.refresh_rate << "hz  "
                << SDL_GetPixelFormatName(mode.format) << "\n";
    }
  }
  ivec2 window_size = {1280, 720};
  int32 flags = SDL_WINDOW_OPENGL;
  // SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_Window *window =
      SDL_CreateWindow("title", 100, 130, window_size.x, window_size.y, flags);

  SDL_GLContext context = SDL_GL_CreateContext(window);

  int32 major, minor;
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
  std::cout << "OpenGL Version: " << major << "." << minor << "\n";
  if (major <= 3)
  {
    if (major < 3 || minor < 3)
    {
      std::cout << "Unsupported OpenGL Version.\n";
      ASSERT(0);
    }
  }
  //recommend keeping vsync off until renderer implements extrapolation
  //int32 swap = SDL_GL_SetSwapInterval(-1);//1 vsync, 0 no vsync, -1 late-swap
  //if (swap == -1)
  //{
  //  swap = SDL_GL_SetSwapInterval(1);
  //}
  SDL_GL_SetSwapInterval(0);
  GLenum err = glewInit();
  if (err != GLEW_OK)
  {
    std::cout << "Glew error: " << glewGetErrorString(err);
    ASSERT(0);
  }
  checkSDLError(__LINE__);
  while (glGetError())
  {
  }

  ivec2 mouse_position;

  SDL_ClearError();
  Uint64 frame_count = 0;
  float64 last_time = 0.0;
  float64 elapsed_time = 0.0;
  State state(window, window_size);
  while (state.running)
  {
    float64 time = get_time();
    elapsed_time = time - state.current_time;

    if (elapsed_time > 0.3)
      elapsed_time = 0.3;
    last_time = state.current_time;

    while (state.current_time + dt < last_time + elapsed_time)
    {
      state.current_time += dt;

      state.handle_input();
      state.update();
    }
    time = get_time(); // more accurate render lerp
    float64 t = (time - state.current_time) / dt;

    state.render(t);
    ++frame_count;
    performance_output(&state, time, frame_count);
    // SDL_Delay(500);
  }
  SDL_Quit();
  return 0;
}
