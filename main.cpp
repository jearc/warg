#include <GL/glew.h>
#include <SDL2/SDL.h>

int main(void)
{
  GLuint i = 0;

  SDL_Init(SDL_INIT_EVERYTHING);
  auto window = SDL_CreateWindow("title", 0, 0, 320, 240, 0);
  auto context = SDL_GL_CreateContext(window);
  glewInit();
  glClearColor(0, 1, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  SDL_GL_SwapWindow(window);
  while (1)
  {
  }

  return i;
}
