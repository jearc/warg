#pragma once

#include "Render.h"
#include "Scene_Graph.h"

class State
{
public:
  State(SDL_Window *window, ivec2 window_size);

  void render(float64 t);
  void update();
  void handle_input();
  float64 current_time = 0;
  void reset_mouse_delta();
  bool running = true;

  // definitely memory leaking on destruction ATM but will be fixed eventually,
  // probably
  Render renderer;

  // scene graph should cleanup everything(gpu too) if overwritten by another
  // scene
  // but not tested
  Scene_Graph scene; // holds and manages all 3D model data and pos/orient/scale

private:
  void prepare_renderer(double t);
  SDL_Window *window;
  ivec2 mouse_position = ivec2(0, 0);
  vec3 camera_position = vec3(-2.0, -2.0, 1.3);
  vec3 camera_gaze_dir;
  float camera_x_radians = atan2(-camera_position.y, -camera_position.x);
  float camera_y_radians = 0.0f;

  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(1, 0, 0);
  Node_Ptr player_mesh = Node_Ptr(-1);
};
