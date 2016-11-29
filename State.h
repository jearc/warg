#pragma once

#include "Render.h"
#include "Scene_Graph.h"
#include <functional>
#include <vector>

struct Entity
{
  Node_Ptr node = Node_Ptr(-1);
  Light *light = nullptr;
  std::function<void(Scene_Graph &, Entity &, float64)> update;
};

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

  Render renderer;

  Scene_Graph scene;

private:
  void prepare_renderer(double t);
  SDL_Window *window = nullptr;
  ivec2 mouse_position = ivec2(0, 0);
  uint32 previous_mouse_state = 0;
  float32 cam_theta = 0;
  float32 cam_phi = half_pi<float32>()*0.5f;
  float32 cam_zoom = 4;
  vec3 cam_pos;
  vec3 cam_dir;
  bool cam_free = false;

  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(0, 1, 0);
  Node_Ptr player_mesh = Node_Ptr(-1);
};
