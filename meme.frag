#version 330

in vec3 frag_kolor;
in vec2 ndc_pos;

uniform vec2 mouse;

layout(location = 0) out vec4 out0;

void main()
{
	out0 = vec4(frag_kolor, 1);
	out0 += vec4(vec3(1, 1, 1) / length(ndc_pos - mouse) / 10, 1);
}