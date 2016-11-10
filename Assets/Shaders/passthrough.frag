#version 330
uniform sampler2D albedo;

in vec2 frag_uv;

#define ALBEDO_TARGET gl_FragData[0]
#define NORMAL_TARGET gl_FragData[1]
#define POSITION_TARGET gl_FragData[2]

void main() { 

ALBEDO_TARGET = texture2D(albedo, frag_uv); 

}