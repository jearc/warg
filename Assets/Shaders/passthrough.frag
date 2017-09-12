#version 330
uniform sampler2D albedo;

in vec2 frag_uv;

layout(location = 0) out vec4 ALBEDO;

void main()
{
  ALBEDO = texture2D(albedo, frag_uv);
}