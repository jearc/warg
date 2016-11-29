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
  material.albedo = "pebbles_diffuse.png";
  material.emissive = "";
  material.normal = "pebbles_normal.png";
  material.roughness = "pebbles_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";

  auto ground_mesh =
      scene.add_single_mesh("plane", "test_entity_plane", material);
  Scene_Graph_Node *n = scene.get_node(ground_mesh);
  ASSERT(n);
  n->position = {0.0f, 0.0f, 0.0f};
  n->scale = {10.0f, 10.0f, 1.0f};

  auto light = scene.lights[scene.light_count++];
  light.position = vec3{0, 0, 10};
  light.color = 40.0f * vec3(1.0f, 0.93f, 0.92f);
  light.attenuation = vec3(1.0f, .045f, .0075f);
  light.ambient = 0.000f;
  light.type = omnidirectional;

  material.albedo = "crate_diffuse.png";
  material.emissive = "test_emissive.png";
  material.normal = "test_normal.png";
  material.roughness = "crate_roughness.png";
  material.vertex_shader = "vertex_shader.vert";
  material.frag_shader = "fragment_shader.frag";

  player_mesh = scene.add_single_mesh("cube", "test_entity_cube", material);

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
  renderer.set_camera(cam_pos, cam_dir);

  // Traverse graph nodes and submit to renderer for packing:
  auto render_entities = scene.visit_nodes_async_start();
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
    else if (_e.type == SDL_KEYUP)
    {
      if (_e.key.keysym.sym == SDLK_F1)
      {
        cam_free = !cam_free;
      }
    }
    else if (_e.type == SDL_MOUSEWHEEL)
    {
      if (_e.wheel.y < 0)
        cam_zoom += 0.1f;
      else if (_e.wheel.y > 0)
        cam_zoom -= 0.1f;
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

  vec2 mouse_angle =
      vec2(-mouse_delta.x * MOUSE_X_SENS, mouse_delta.y * MOUSE_Y_SENS);

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

  if (cam_free)
  {
    mat4 rx = rotate(mouse_angle.x, vec3(0, 0, 1));
    cam_dir = vec3(rx * vec4(cam_dir, 0));
    mat4 ry = rotate(mouse_angle.y, vec3(-cam_dir.y, cam_dir.x, 0));
    cam_dir = vec3(ry * rx * vec4(cam_dir, 0));

    if (is_pressed(SDL_SCANCODE_W))
      cam_pos += MOVE_SPEED * cam_dir;
    if (is_pressed(SDL_SCANCODE_S))
      cam_pos -= MOVE_SPEED * cam_dir;
    if (is_pressed(SDL_SCANCODE_D))
      cam_pos += MOVE_SPEED * vec3(rotate(-half_pi<float>(), vec3(0, 0, 1)) *
                                   vec4(cam_dir.x, cam_dir.y, 0, 0));
    if (is_pressed(SDL_SCANCODE_A))
      cam_pos += MOVE_SPEED * vec3(rotate(half_pi<float>(), vec3(0, 0, 1)) *
                                   vec4(cam_dir.x, cam_dir.y, 0, 0));
  }
  else
  {
    if (left_button_down || right_button_down)
    {

      mat4 rx = rotate(mouse_angle.x, vec3(0, 0, 1));
      mat4 ry = rotate(mouse_angle.y, vec3(0, 1, 0));

      cam_rel = vec3(ry * rx * vec4(cam_rel.x, cam_rel.y, cam_rel.z, 0));

      if (right_button_down)
      {
        player_dir = -cam_rel;
      }
    }

    if (is_pressed(SDL_SCANCODE_W))
      player_pos += MOVE_SPEED * vec3(player_dir.x, player_dir.y, 0.0f);
    if (is_pressed(SDL_SCANCODE_S))
      player_pos -= MOVE_SPEED * vec3(player_dir.x, player_dir.y, 0.0f);
    if (is_pressed(SDL_SCANCODE_A))
      player_pos += MOVE_SPEED * vec3(rotate(half_pi<float>(), vec3(0, 0, 1)) *
                                      vec4(player_dir.x, player_dir.y, 0, 0));
    if (is_pressed(SDL_SCANCODE_D))
      player_pos += MOVE_SPEED * vec3(rotate(-half_pi<float>(), vec3(0, 0, 1)) *
                                      vec4(player_dir.x, player_dir.y, 0, 0));

    cam_pos = player_pos + cam_rel * cam_zoom;
    cam_dir = -cam_rel;
  }
}

void State::reset_mouse_delta()
{
  ivec2 mouse_delta;
  SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);
}

void State::update()
{
  Scene_Graph_Node *n = scene.get_node(player_mesh);
  ASSERT(n);

  n->position = player_pos;
  n->scale = vec3(1.0f);
  n->orientation = player_dir;
}

void State::render(float64 t)
{
  check_gl_error();
  prepare_renderer(t);
  renderer.render(t);
}

Light *State::add_light() { return &scene.lights[scene.light_count++]; }
