#version 330

layout(location = 0) out vec4 out0;

uniform vec4 color;

void main()
{
	out0 = color;
}