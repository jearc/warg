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

const float PI = 3.14159265358979f;
const float gamma = 2.2;

vec3 to_linear(in vec3 srgb) { return pow(srgb, vec3(gamma)); }
float to_linear(in float srgb) { return pow(srgb, gamma); }
vec3 to_srgb(in vec3 linear) { return pow(linear, vec3(1 / gamma)); }

float linearize_depth(float z)
{
  float near = 0.1;
  float far = 1000;
  float depth = z * 2.0 - 1.0;
  float linearDepth = (2.0 * near * far) / (far + near - depth * (far - near));
  return linearDepth;
}


struct Material
{
  vec3 albedo;
  vec3 specular;
  vec3 emissive;
  vec3 normal;
  float shininess;
};

void main()
{
  Material m;
    
  m.specular = to_linear(texture2D(specular, frag_uv).rgb);
  // float albedo_scale = 1.0 - m.specular.r;
  m.albedo = to_linear(texture2D(albedo, frag_uv).rgb) / PI;
  // m.albedo = m.albedo*albedo_scale;
  m.emissive = to_linear(texture2D(emissive, frag_uv).rgb);
  m.shininess = (1.0 - to_linear(texture2D(roughness, frag_uv).r));
  vec3 tex_normal = texture2D(normal, frag_uv).rgb;
  if (tex_normal == vec3(0))
  { // missing normal map, set to perp of triangle
    tex_normal = vec3(0, 0, 1);
  }
  m.normal = frag_TBN * normalize((tex_normal * 2) - 1.0f);

  
  m.shininess = 1.0 + m.shininess * 44.0f;


  vec3 debug;
  vec3 result = vec3(0);
  for (int i = 0; i < number_of_lights; ++i)
  {
    vec3 light_p = lights[i].position;
    vec3 to_light = light_p - frag_world_position;
    float dist = length(to_light);
    to_light = normalize(to_light);
    vec3 to_eye = normalize(camera_position - frag_world_position);
    vec3 half_v = normalize(to_light + to_eye);

    vec3 att = lights[i].attenuation;
    float at = 1.0 / (att.x + (att.y * dist) + (att.z * dist * dist));
    vec3 ambient = lights[i].ambient * m.albedo * att;
    result += ambient;
    float alpha = 1.0f;
    if (lights[i].type == 0)
    { // directional
      to_light = -lights[i].direction;
      half_v = normalize(to_light + to_eye);
    }
    else if (lights[i].type == 2)
    { // cone
      vec3 light_gaze = normalize(lights[i].position - lights[i].direction);
      float theta = lights[i].cone_angle;
      float phi = 1.0 - dot(to_light, light_gaze);
      alpha = 0.0f;
      if (phi < theta)
      {
        float edge_softness_distance = 0.3f;
        alpha = clamp((theta - phi) / edge_softness_distance, 0, 1);
      }
    }
    float ldotn = clamp(dot(to_light, m.normal), 0, 1);
    float energy_conservation = (8.0f * m.shininess) / (8.0f * PI);
    float specular =
        energy_conservation * pow(max(dot(half_v, m.normal), 0.0), m.shininess);
    result += ldotn * specular * m.albedo * lights[i].color * at * alpha;
 
  }
  result += m.emissive;
  result += vec3(0.25) * additional_ambient * m.albedo;

  // debug = frag_world_position/100;
  // debug = vec3(frag_uv,0);
  // debug = m.albedo;
  if (debug != vec3(0))
  {
    result = debug;
  }
  // result = vec3(0,1,0);
  gl_FragColor = vec4(to_srgb(result), 1);
}
