#version 330

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 kolor;

uniform mat4 transform;

out vec3 frag_kolor;
out vec2 ndc_pos;

void main()
{
	vec2 pos = (transform * vec4(position, 0, 1)).xy;
	gl_Position = vec4(pos, 0, 1);
	frag_kolor = kolor;
	ndc_pos = pos;
}