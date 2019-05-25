#version 330

in vec3 frag_kolor;
in vec2 ndc_pos;
in vec2 img_pos;

uniform vec2 mouse;
uniform float aspect_ratio;
uniform sampler2D texture0;

layout(location = 0) out vec4 out0;

void main()
{
	vec2 len = ndc_pos - mouse;
	len.x *= aspect_ratio;
	//out0 = vec4(frag_kolor, 1);
	vec4 mod = vec4(vec3(1, 1, 1) / length(len), 1);
	if (length(len) > 0.5) mod = vec4(0.f);
	
	out0 += mod * texture2D(texture0, img_pos);
}