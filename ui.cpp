#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
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
	float vertices[] = { -0.33, 0.33, 0.33, 0, -0.33, -0.33 };
	// clang-format off
	float kolors[] = {
		1, 0, 0,
		0, 1, 0,
		0, 0, 1
	};
	// clang-format on

	glGenBuffers(2, data.buf);

	glBindBuffer(GL_ARRAY_BUFFER, data.buf[0]);
	glBufferData(
		GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, data.buf[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof kolors, kolors, GL_STATIC_DRAW);

	glGenVertexArrays(1, &data.vao);
	glBindVertexArray(data.vao);

	glBindBuffer(GL_ARRAY_BUFFER, data.buf[0]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, data.buf[1]);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(1);

	data.shader = load_shader(vert, frag);

	data.vertex_transform_location =
		glGetUniformLocation(data.shader, "transform");
	data.mouse_location = glGetUniformLocation(data.shader, "mouse");

	return data;
}

void de_init_ui(ui_data data)
{
	glDeleteBuffers(2, data.buf);
	glDeleteVertexArrays(1, &data.vao);
	glDeleteShader(data.shader);
}