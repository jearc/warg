#include "State.h"
#include "Globals.h"
#include "Render.h"
#include <atomic>
#include <thread>

using namespace glm;

State::State(SDL_Window *window, ivec2 window_size)
    : window(window), renderer(window, window_size)
{
  Material_Descriptor material;
  material.albedo = "crate_diffuse.png";
  material.emissive = "test_emissive.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";

  test_entity_cube =
      scene.add_single_mesh("cube", "test_entity_cube", material);
  test_entity_light =
      scene.add_single_mesh("cube", "test_entity_light", material);
  test_entity_light_cone =
      scene.add_single_mesh("cube", "test_entity_light_cone", material);
  test_entity_light_large =
      scene.add_single_mesh("cube", "test_entity_light_large", material);

  material.albedo = "pebbles_diffuse.png";
  material.emissive = "";
  material.normal = "pebbles_normal.png";
  material.roughness = "pebbles_roughness.png";

  test_entity_plane =
      scene.add_single_mesh("plane", "test_entity_plane", material);


  illidan = scene.add_aiscene("Chest/Chest1.FBX");

  SDL_SetRelativeMouseMode(SDL_bool(true));
  reset_mouse_delta();
  check_gl_error();
}

void State::prepare_renderer(double t)
{
  // frustrum cull using primitive bounding volumes
  // determine which lights affect which objects

  /*Light diameter guideline for deferred rendering (not yet used)
  Distance 	Constant 	Linear 	Quadratic
7 	1.0 	0.7 	1.8
13 	1.0 	0.35 	0.44
20 	1.0 	0.22 	0.20
32 	1.0 	0.14 	0.07
50 	1.0 	0.09 	0.032
65 	1.0 	0.07 	0.017
100 	1.0 	0.045 	0.0075
160 	1.0 	0.027 	0.0028
200 	1.0 	0.022 	0.0019
325 	1.0 	0.014 	0.0007
600 	1.0 	0.007 	0.0002
3250 	1.0 	0.0014 	0.000007

  */

  // camera must be set before entities, or they get a 1 frame lag
  renderer.set_camera(camera_position, camera_gaze_dir);

  // Traverse graph nodes and submit to renderer for packing:
  auto render_entities = scene.visit_nodes_st_start();
  renderer.set_render_entities(render_entities);
}

void State::handle_input()
{
  auto is_pressed = [](int key) {
    const static Uint8 *keys = SDL_GetKeyboardState(NULL);
    SDL_PumpEvents();
    return keys[key];
  };
  SDL_Event _e;
  while (SDL_PollEvent(&_e))
  {
    if (_e.type == SDL_QUIT)
    {
      running = false;
      return;
    }
    else if (_e.type == SDL_KEYDOWN)
    {
      if (_e.key.keysym.sym == SDLK_ESCAPE)
      {
        running = false;
        return;
      }
    }
    else if (_e.type == SDL_WINDOWEVENT)
    {
      if (_e.window.event == SDL_WINDOWEVENT_RESIZED)
      {
        // resize
      }
      else if (_e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
               _e.window.event == SDL_WINDOWEVENT_ENTER)
      { // dumping mouse delta prevents camera teleport on focus gain

        // required, else clicking the title bar itself to gain focus
        // makes SDL ignore the mouse entirely for some reason...
        SDL_SetRelativeMouseMode(SDL_bool(false));
        SDL_SetRelativeMouseMode(SDL_bool(true));
        reset_mouse_delta();
      }
    }
  }

  checkSDLError(__LINE__);
  // first person style camera control:
  const uint32 mouse_state =
      SDL_GetMouseState(&mouse_position.x, &mouse_position.y);

  // note: SDL_SetRelativeMouseMode is being set by the constructor
  ivec2 mouse_delta;
  SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);

  float32 radians_x = -mouse_delta.x * MOUSE_X_SENS;
  float32 radians_y = mouse_delta.y * MOUSE_Y_SENS;

  camera_x_radians += radians_x;
  camera_y_radians += radians_y;

  camera_y_radians = clamp(camera_y_radians, -half_pi<float32>() + 0.0001f,
                           half_pi<float32>() - 0.0001f);

  mat4 rx = rotate(camera_x_radians, vec3(0, 0, 1));
  vec4 heading = rx * vec4(1, 0, 0, 0);
  mat4 ry = rotate(camera_y_radians, vec3(-heading.y, heading.x, 0));

  camera_gaze_dir = vec3(ry * rx * vec4(1, 0, 0, 0));

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);

  if (is_pressed(SDL_SCANCODE_W))
    camera_position += MOVE_SPEED * camera_gaze_dir;
  if (is_pressed(SDL_SCANCODE_S))
    camera_position -= MOVE_SPEED * camera_gaze_dir;
  if (is_pressed(SDL_SCANCODE_D))
  {
    camera_position +=
        MOVE_SPEED * vec3(rotate(-half_pi<float>(), vec3(0, 0, 1)) * heading);
  }
  if (is_pressed(SDL_SCANCODE_A))
  {
    camera_position +=
        MOVE_SPEED * vec3(rotate(half_pi<float>(), vec3(0, 0, 1)) * heading);
  }
}

void State::reset_mouse_delta()
{
  ivec2 mouse_delta;
  SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);
}

void State::update()
{
  Scene_Graph_Node *test_entity_cube = scene.get_node(this->test_entity_cube);
  Scene_Graph_Node *test_entity_light = scene.get_node(this->test_entity_light);
  Scene_Graph_Node *test_entity_light_cone =
      scene.get_node(this->test_entity_light_cone);
  Scene_Graph_Node *test_entity_light_large =
      scene.get_node(this->test_entity_light_large);
  Scene_Graph_Node *test_entity_plane = scene.get_node(this->test_entity_plane);

  ASSERT(test_entity_cube);
  ASSERT(test_entity_light);
  ASSERT(test_entity_light_cone);
  ASSERT(test_entity_light_large);
  ASSERT(test_entity_plane);

  float32 x, y, z;

  x = 32.0f * cos(float32(current_time) / 3.0f);
  y = 0.0f;
  z = 32.0f * sin(float32(current_time) / 3.0f);
  test_entity_light_large->scale = vec3(.15f);
  test_entity_light_large->position = vec3(x, y, z);

  test_entity_plane->scale = {10.0f, 10.0f, 1.0f};

  test_entity_cube->position = {0.0f, 0.0f, 0.9f};
  test_entity_cube->scale = {1.0f, 1.0f, 1.0f};
  test_entity_cube->orientation.x += 0.0031f;
  test_entity_cube->orientation.z += 0.0031f;

  x = 1.2f * cos(float32(current_time) * 2.0f);
  y = 1.2f * sin(float32(current_time) * 2.0f);
  z = 0.91f + 0.9f * sin(float32(current_time));
  test_entity_light->position = {x, y, z};
  test_entity_light->scale = {0.15f, 0.15f, 0.15f};

  x = 5.0f * cos(float32(current_time));
  y = 5.0f * sin(float32(current_time));
  z = 0.7f;
  test_entity_light_cone->position = {x, -y, z};
  test_entity_light_cone->scale = {.15f, .15f, .15f};

  const float32 intensity = 20.0f;

  scene.lights[0].position = test_entity_light->position;
  scene.lights[0].color = intensity * vec3(1.0, 0.2, 0.2);
  scene.lights[0].attenuation = vec3(1, .7, 1.8);
  scene.lights[0].ambient = 0.0f;
  scene.lights[0].type = omnidirectional;

  scene.lights[1].position = test_entity_light_cone->position;
  scene.lights[1].color = 5.0f * intensity * vec3(0.6f, 0.6f, 0.9f);
  scene.lights[1].direction = vec3(0.0f, 0.0f, 0.2f);
  scene.lights[1].attenuation = vec3(1.0f, .14f, .07f);
  scene.lights[1].ambient = 0.000f;
  scene.lights[1].cone_angle = 0.20f;
  scene.lights[1].type = spot;

  scene.lights[2].position = test_entity_light_large->position;
  scene.lights[2].color = 1.0f * intensity * vec3(1.0f, 0.93f, 0.92f);
  scene.lights[2].attenuation = vec3(1.0f, .045f, .0075f);
  scene.lights[2].ambient = 0.000f;
  scene.lights[2].type = omnidirectional;

  scene.light_count = 3;
}
void State::render(float64 t)
{
  check_gl_error();
  prepare_renderer(t);
  renderer.render(t, current_time);
}
