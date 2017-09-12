#version 330
uniform sampler2D albedo;
uniform sampler2D specular;
uniform sampler2D normal;
uniform sampler2D emissive;
uniform sampler2D roughness;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 additional_ambient;
uniform float time;
uniform vec3 camera_position;
uniform vec2 uv_scale;
struct Light
{
  vec3 position;
  vec3 direction;
  vec3 color;
  vec3 attenuation;
  vec3 ambient;
  float cone_angle;
  uint type;
};
#define MAX_LIGHTS 10
uniform Light lights[MAX_LIGHTS];
uniform uint number_of_lights;

in vec3 frag_world_position;
in mat3 frag_TBN;
in vec2 frag_uv;
layout(location = 0) out vec4 ALBEDO;
#define gamma 2.2

float linearize_depth(float depth)
{
  float near = 0.1;
  float far = 10000;
  float z = depth * 2.0 - 1.0;
  return (2.0 * near * far) / (far + near - z * (far - near));
}
vec3 to_srgb(in vec3 linear) { return pow(linear, vec3(1 / gamma)); }

void main()
{
  vec2 scale = vec2(25, 25);
  bool tile_light = (mod(scale.x * frag_uv.x, 1) < 0.5) ^^ (mod(scale.y * frag_uv.y, 1) < 0.5);
  float tile_color = 0.0f + 1.0f * float(tile_light);
  float fade = length(frag_world_position / 40);
  fade = pow(fade, 2);
  tile_color -= fade;

  vec3 color = vec3(tile_color);
  if (tile_light)
  {
    color = frag_world_position / 12;
  }

    float dist = length(frag_world_position);  
    if(dist < 1)
    {
      color += 0.5f*vec3(1.000f+sin(time*10)*dist);
    }
    

  ALBEDO = vec4(to_srgb(color), 1);
}