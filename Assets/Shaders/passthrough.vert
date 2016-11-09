#version 330
in vec3 position;
in vec2 uv;

uniform mat4 transform;

out vec2 frag_uv;
void main()
{
  frag_uv = uv;
  gl_Position = transform * vec4(position, 1);
}