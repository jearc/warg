#version 330

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 kolor;
layout(location = 2) in vec2 texture_coord;

uniform mat4 transform;

out vec3 frag_kolor;
out vec2 ndc_pos;
out vec2 img_pos;

void main()
{
	vec2 pos = (vec4(position, 0, 1)).xy;
	gl_Position = vec4(pos, 0, 1);
	frag_kolor = kolor;
	ndc_pos = pos;
	img_pos = texture_coord;
}