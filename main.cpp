#include "Globals.h"
#include "Render.h"
#include "State.h"
#include "Timer.h"
#include <GL/glew.h>
#include <SDL.h>
#undef main
#include <iostream>
#include <stdlib.h>

static void performance_output(const State &state)
{
  static float64 last_output = 0;
  static uint64 frames_at_last_tick = 0;
  const float64 report_delay = 1.0;
  const uint64 frame_count = state.renderer.frame_count;
  const uint64 frames_since_last_tick = frame_count - frames_at_last_tick;
  const float64 current_time = state.current_time;

  if (last_output + report_delay < state.current_time)
  {
#ifdef __linux__
    system("clear");
#elif _WIN32
    system("cls");
#endif
    frames_at_last_tick = frame_count;
    last_output = current_time;
    Uint64 current_frame_rate = (1.0 / report_delay) * frames_since_last_tick;
    std::cout << PERF_TIMER.string_report();
    std::cout << "FPS: " << current_frame_rate << "\n";
    std::cout << "Total FPS:" << (float64)frame_count / current_time;
    std::cout << "\nRender Scale: " << state.renderer.get_render_scale();
    std::cout << "\nDraw calls: " << state.renderer.render_instances.size();
    std::cout << "\n\nMessages:\n\n" << get_messages() << std::endl;
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
      throw;
    }
  }
  // 1 vsync, 0 no vsync, -1 late-swap
  int32 swap = SDL_GL_SetSwapInterval(1); 
  if (swap == -1)
  {
    swap = SDL_GL_SetSwapInterval(1);
  }

  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  glClearColor(0, 0, 0, 1);
  if (err != GLEW_OK)
  {
    std::cout << "Glew error: " << glewGetErrorString(err);
    ASSERT(0);
  }
  checkSDLError(__LINE__);
  while (glGetError())
  {
  }
  SDL_ClearError();
  float64 last_time = 0.0;
  float64 elapsed_time = 0.0;
  State state(window, window_size);
  while (state.running)
  {
    float64 time = get_real_time();
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
    state.render(state.current_time);
    performance_output(state);
    // SDL_Delay();
  }
  SDL_Quit();
  return 0;
}
