struct ui_data {
	GLuint shader;
	GLuint vao_quad;
	GLuint vao_tri;
	GLuint vao_pause;
	GLuint uniform_transform;
	GLuint uniform_color;
};

ui_data ui_init();
void render_ui(ui_data *d, float aspect_ratio);