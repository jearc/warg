#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <pango/pangocairo.h>
#include "moov.h"
#include "ui.h"

struct mat4x4 {
	float arr[4][4];
};

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
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	ui_data d;
	
	GLuint buf;
	
	float quad[] = { -1.0, -1.0, 1.0, -1.0, 1.0, 1.0,
	                 -1.0, -1.0, 1.0, 1.0, -1.0, 1.0
	};
	glGenVertexArrays(1, &d.vao_quad);
	glBindVertexArray(d.vao_quad);
	glGenBuffers(1, &buf);
	glBindBuffer(GL_ARRAY_BUFFER, buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(0);
	
	float tri[] = { -1.0, -1.0, 1.0, 0.0, -1.0, 1.0 };
	glGenVertexArrays(1, &d.vao_tri);
	glBindVertexArray(d.vao_tri);
	glGenBuffers(1, &buf);
	glBindBuffer(GL_ARRAY_BUFFER, buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(0);
	
	float pause[] = { -1.0, 1.0, -1.0, -1.0, -0.33, -1.0,
	                  -0.33, -1.0, -0.33, 1.0, -1.0, 1.0,
	                  0.33, 1.0, 0.33, -1.0, 1.0, -1.0,
	                  1.0, -1.0, 1.0, 1.0, 0.33, 1.0
	};
	glGenVertexArrays(1, &d.vao_pause);
	glBindVertexArray(d.vao_pause);
	glGenBuffers(1, &buf);
	glBindBuffer(GL_ARRAY_BUFFER, buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof pause, pause, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(0);
	
	d.shader = load_shader(vert, frag);
	
	d.uniform_color = glGetUniformLocation(d.shader, "color");
	d.uniform_transform = glGetUniformLocation(d.shader, "transform");
	
	return d;
}

void mul(mat4x4 a, mat4x4 *b)
{
	mat4x4 c = {0};
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			for (int m = 0; m < 4; m++)
				c.arr[j][i] += a.arr[m][i] * b->arr[j][m];
	*b = c;
}

mat4x4 identity()
{
	mat4x4 a = {0};
	a.arr[0][0] = 1;
	a.arr[1][1] = 1;
	a.arr[2][2] = 1;
	a.arr[3][3] = 1;
	return a;
}

void scale(mat4x4 *m, float x, float y)
{
	mat4x4 a = identity();
	a.arr[0][0] = x;
	a.arr[1][1] = y;
	mul(a, m);
}

void translate(mat4x4 *m, float x, float y)
{
	mat4x4 a = identity();
	a.arr[3][0] = x;
	a.arr[3][1] = y;
	mul(a, m);
}

void order(mat4x4 *m, int n)
{
	mat4x4 a = identity();
	a.arr[3][2] = (float)n * -0.000001;
	mul(a, m);
}

void render_ui(ui_data *d, float aspect_ratio)
{
	glUseProgram(d->shader);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);
	
	mat4x4 tbar, tplay, tpause;
	tbar = tplay = tpause = identity();
	
	float padding = 0.25;
	
	glBindVertexArray(d->vao_quad);
	translate(&tbar, 1.0, 1.0);
	scale(&tbar, 1.0, 1.0 / 40 * aspect_ratio);
	translate(&tbar, -1.0, -1.0);
	order(&tbar, 1);
	glUniformMatrix4fv(d->uniform_transform, 1, GL_FALSE,
		(float *)tbar.arr);
	glUniform4f(d->uniform_color, 0.0, 0.0, 0.0, 0.7);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	glBindVertexArray(d->vao_tri);
	scale(&tplay, 1.0 - 2.0 * padding, 1.0 - 2.0 * padding);
	translate(&tplay, 1.0, 0.0);
	scale(&tplay, 0.87 / 40, 1);
	translate(&tplay, -1.0, 0.0);
	mul(tbar, &tplay);
	order(&tplay, 2);
	glUniformMatrix4fv(d->uniform_transform, 1, GL_FALSE,
		(float *)tplay.arr);
	glUniform4f(d->uniform_color, 1.0, 1.0, 1.0, 1.0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	
	glBindVertexArray(d->vao_pause);
	scale(&tpause, 1.0 - 2.0 * padding, 1.0 - 2.0 * padding);
	translate(&tpause, 1.0, 0.0);
	scale(&tpause, 1.0 * 0.5/ 40, 1);
	translate(&tpause, -1.0 + 2.0 * 0.87 / 40, 0.0);
	mul(tbar, &tpause);
	order(&tpause, 2);
	glUniformMatrix4fv(d->uniform_transform, 1, GL_FALSE,
		(float *)tpause.arr);
	glUniform4f(d->uniform_color, 1.0, 1.0, 1.0, 1.0);
	glDrawArrays(GL_TRIANGLES, 0, 12);
	
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
}