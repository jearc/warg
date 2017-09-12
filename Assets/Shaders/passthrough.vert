#version 330
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;

uniform mat4 transform;

out vec2 frag_uv;
void main()
{
  frag_uv = uv;
  gl_Position = transform * vec4(position, 1);
}