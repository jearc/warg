#version 330
uniform sampler2D albedo;

in vec2 uv;

#define DIFFUSE_TARGET gl_FragData[0]
#define NORMAL_TARGET gl_FragData[1]
#define POSITION_TARGET gl_FragData[2]

void main() { DIFFUSE_TARGET = texture2D(albedo, uv); }