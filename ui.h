struct ui_data {
	GLuint buf[2];
	GLuint vao;
	GLuint shader;
	GLuint vertex_transform_location;
	GLuint mouse_location;
};

ui_data ui_init();