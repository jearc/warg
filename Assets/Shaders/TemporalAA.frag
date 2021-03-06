#version 330
uniform sampler2D current;
uniform sampler2D previous;

in vec2 frag_uv;
layout(location = 0) out vec4 ALBEDO;
void main()
{
  vec4 previous = texture2D(previous, frag_uv);
  vec4 current = texture2D(current, frag_uv);
  vec4 result = mix(previous, current, 0.5f);
  ALBEDO = result;
}