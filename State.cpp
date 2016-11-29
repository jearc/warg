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
        cam_zoom += ZOOM_STEP;
      else if (_e.wheel.y > 0)
        cam_zoom -= ZOOM_STEP;
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

  bool left_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool right_button_down = mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);
  bool last_seen_lmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT);
  bool last_seen_rmb = previous_mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT);

  if (cam_free)
  {
    cam_theta += mouse_delta.x * MOUSE_X_SENS;
    cam_phi += mouse_delta.y * MOUSE_Y_SENS;
    // wrap x
    if (cam_theta > two_pi<float32>())
      cam_theta = cam_theta - two_pi<float32>();
    if (cam_theta < 0)
      cam_theta = two_pi<float32>() + cam_theta;
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (cam_phi > upper)
      cam_phi = upper;
    const float32 lower = -half_pi<float32>() + 100 * epsilon<float32>();
    if (cam_phi < lower)
      cam_phi = lower;

    mat4 rx = rotate(-cam_theta, vec3(0, 0, 1));
    vec4 vr = rx * vec4(0, 1, 0, 0);
    vec3 perp = vec3(-vr.y, vr.x, 0);
    mat4 ry = rotate(cam_phi, perp);
    cam_dir = normalize(vec3(ry * vr));

    if (is_pressed(SDL_SCANCODE_W))
      cam_pos += MOVE_SPEED * cam_dir;
    if (is_pressed(SDL_SCANCODE_S))
      cam_pos -= MOVE_SPEED * cam_dir;
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.xy, 0, 0);
      cam_pos += vec3(MOVE_SPEED * r * v);
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(vr.xy, 0, 0);
      cam_pos += vec3(MOVE_SPEED * r * v);
    }
  }
  else
  { // wow style camera
    vec4 cam_rel;
    if ((left_button_down || right_button_down) &&
        (last_seen_lmb || last_seen_rmb))
    { // won't track mouse delta that happened when mouse button was not pressed
      cam_theta += mouse_delta.x * MOUSE_X_SENS;
      cam_phi += mouse_delta.y * MOUSE_Y_SENS;
    }
    // wrap x
    if (cam_theta > two_pi<float32>())
      cam_theta = cam_theta - two_pi<float32>();
    if (cam_theta < 0)
      cam_theta = two_pi<float32>() + cam_theta;
    // clamp y
    const float32 upper = half_pi<float32>() - 100 * epsilon<float32>();
    if (cam_phi > upper)
      cam_phi = upper;
    const float32 lower = 100 * epsilon<float32>();
    if (cam_phi < lower)
      cam_phi = lower;

    //+x right, +y forward, +z up

    // construct a matrix that represents a rotation around the z axis by
    // theta(x) radians
    mat4 rx = rotate(-cam_theta, vec3(0, 0, 1));

    //'default' position of camera is behind the character
    // rotate that vector by our rx matrix
    cam_rel = rx * normalize(vec4(0, -1, 0.0, 0));

    // perp is the camera-relative 'x' axis that moves with the camera
    // as the camera rotates around the character-centered y axis
    // should always point towards the right of the screen
    vec3 perp = vec3(-cam_rel.y, cam_rel.x, 0);

    // construct a matrix that represents a rotation around perp by -phi
    mat4 ry = rotate(-cam_phi, perp);

    // rotate the camera vector around perp
    cam_rel = normalize(ry * cam_rel);

    if (right_button_down)
    {
      player_dir = normalize(-vec3(cam_rel.xy, 0));
    }
    if (is_pressed(SDL_SCANCODE_W))
    {
      vec3 v = vec3(player_dir.xy, 0.0f);
      player_pos += MOVE_SPEED * v;
    }
    if (is_pressed(SDL_SCANCODE_S))
    {
      vec3 v = vec3(player_dir.xy, 0.0f);
      player_pos -= MOVE_SPEED * v;
    }
    if (is_pressed(SDL_SCANCODE_A))
    {
      mat4 r = rotate(half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(player_dir.xy, 0, 0);
      player_pos += MOVE_SPEED * vec3(r * v);
    }
    if (is_pressed(SDL_SCANCODE_D))
    {
      mat4 r = rotate(-half_pi<float>(), vec3(0, 0, 1));
      vec4 v = vec4(player_dir.x, player_dir.y, 0, 0);
      player_pos += MOVE_SPEED * vec3(r * v);
    }
    cam_pos = player_pos + (cam_rel.xyz * cam_zoom);
    cam_dir = -vec3(cam_rel);
  }
  previous_mouse_state = mouse_state;
}

void State::reset_mouse_delta()
{
  ivec2 mouse_delta;
  SDL_GetRelativeMouseState(&mouse_delta.x, &mouse_delta.y);
}

void State::update()
{
  // the idea was to actually use this all over in the code
  // wherever you need to set these variables
  // instead of storing it locally in the state as well
  // for example: in the input handler, you could do
  // scene.get_node(player)
  // and then assign the position from there, directly
  // same with orientation
  // however non-render related things should not be added to the
  // graph nodes, but rather keep the graph node as a component
  // to an entity that needs to be rendered
  Scene_Graph_Node *player = scene.get_node(player_mesh);
  ASSERT(player);

  player->position = player_pos;
  player->scale = vec3(1.0f);
  player->orientation = vec3(0, 0, atan2(player_dir.y, player_dir.x));
}

void State::render(float64 t)
{
  check_gl_error();
  prepare_renderer(t);
  renderer.render(t);
}