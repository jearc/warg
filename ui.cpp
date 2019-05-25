#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "moov.h"
#include "ui.h"

extern const char vert[];
extern const char frag[];

GLuint load_shader(const char *vert, const char *frag)
{
	GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
	GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	GLint res = 0;
	int loglen;
	bool success = true;

	glShaderSource(vert_shader, 1, &vert, NULL);
	glCompileShader(vert_shader);
	glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &res);
	if (!res)
		success = false;
	glGetShaderiv(vert_shader, GL_INFO_LOG_LENGTH, &loglen);

	char *vert_err = (char *)calloc(max(1, loglen), sizeof(GLchar));
	glGetShaderInfoLog(vert_shader, loglen, NULL, vert_err);
	if (loglen > 1)
		fprintf(stderr, "vert shader result: %s\n", vert_err);
	free(vert_err);

	glShaderSource(frag_shader, 1, &frag, NULL);
	glCompileShader(frag_shader);
	glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &res);
	if (!res)
		success = false;

	char *frag_err = (char *)calloc(max(1, loglen), sizeof(GLchar));
	glGetShaderInfoLog(frag_shader, loglen, NULL, frag_err);
	if (loglen > 1)
		fprintf(stderr, "frag shader result: %s\n", frag_err);
	free(frag_err);

	GLuint program = glCreateProgram();
	glAttachShader(program, vert_shader);
	glAttachShader(program, frag_shader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &res);
	if (!res)
		success = false;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &loglen);

	char *prog_err = (char *)calloc(max(1, loglen), sizeof(GLchar));
	glGetProgramInfoLog(program, loglen, NULL, prog_err);
	if (loglen > 1)
		fprintf(stderr, "prog result: %s\n", prog_err);
	free(prog_err);

	glDeleteShader(vert_shader);
	glDeleteShader(frag_shader);

	if (!success)
		die("gl shader failed");

	return program;
}

ui_data ui_init()
{
	ui_data data;
	// clang-format off
	float vertices[] = {
		-0.33, 0.33,
		-0.33, -0.33,
		0.33, 0
	};
	float quadvert[] = {
		-1, 1, //top left
		-1, -1, // bottom left
		1, -1, // bottom right
		1, 1 // top right
	};
	unsigned int quadidx[] = {
		0, 1, 2,
		0, 2, 3
	};
	float quadtex[] = {
		0, 1, // top left
		0, 0, // bottom left
		1, 0, // bottom right
		1, 1 // top right
	};
	float kolors[] = {
		1, 0, 0,
		0, 1, 0,
		0, 0, 1
	};
	// clang-format on

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glGenBuffers(2, data.tribuf);

	glBindBuffer(GL_ARRAY_BUFFER, data.tribuf[0]);
	glBufferData(
		GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, data.tribuf[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof kolors, kolors, GL_STATIC_DRAW);

	glGenBuffers(3, data.quadbuf);

	glBindBuffer(GL_ARRAY_BUFFER, data.quadbuf[0]);
	glBufferData(
		GL_ARRAY_BUFFER, sizeof quadvert, quadvert, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.quadbuf[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof quadidx, quadidx,
		GL_STATIC_DRAW);
		
	glBindBuffer(GL_ARRAY_BUFFER, data.quadbuf[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof quadtex, quadtex, GL_STATIC_DRAW);

	glGenVertexArrays(1, &data.trivao);
	glBindVertexArray(data.trivao);

	glBindBuffer(GL_ARRAY_BUFFER, data.tribuf[0]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ARRAY_BUFFER, data.tribuf[1]);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glGenVertexArrays(1, &data.quadvao);
	glBindVertexArray(data.quadvao);

	glBindBuffer(GL_ARRAY_BUFFER, data.quadbuf[0]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(0);
	
	glBindBuffer(GL_ARRAY_BUFFER, data.quadbuf[2]);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.quadbuf[1]);

	glDisableVertexAttribArray(1);

	data.shader = load_shader(vert, frag);

	data.vertex_transform_location =
		glGetUniformLocation(data.shader, "transform");
	data.mouse_location = glGetUniformLocation(data.shader, "mouse");
	data.aspect_ratio_location =
		glGetUniformLocation(data.shader, "aspect_ratio");
	data.texture0_location = glGetUniformLocation(data.shader, "texture0");

	int w, h, c;
	stbi_set_flip_vertically_on_load(true);
	unsigned char *imgdata = stbi_load(
		"/home/james/meme/twitch/img002818.jpg", &w, &h, &c, 4);
	glGenTextures(1, &data.img_handle);
	glBindTexture(GL_TEXTURE_2D, data.img_handle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, imgdata);
	stbi_image_free(imgdata);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	float border[] = { 0, 1, 0, 0.5 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

	return data;
}
