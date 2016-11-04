#include <GL/glew.h>
#include <SDL2/SDL.h>

int main(void)
{
  int flags = SDL_WINDOW_OPENGL | SDL_RENDERER_PRESENTVSYNC;

  SDL_Init(SDL_INIT_EVERYTHING);
  auto window = SDL_CreateWindow("title", 0, 0, 320, 240, flags);
  auto context = SDL_GL_CreateContext(window);

  glewInit();
  glClearColor(0, 1, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  SDL_Event event;
  while (true)
  {
    while (SDL_PollEvent(&event) != 0)
      if (event.type == SDL_QUIT)
        goto quit;
    SDL_Delay(10);
    SDL_GL_SwapWindow(window);
  }

quit:
  SDL_Quit();
  return 0;
}
