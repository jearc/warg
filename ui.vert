#version 330

layout(location = 0) in vec2 pos;
layout(location = 1) in vec4 color;

uniform mat4 transform;

void main()
{
	gl_Position = transform * vec4(pos, 0, 1);
}