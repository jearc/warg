struct ui_data {
	GLuint tribuf[2];
	GLuint trivao;
	GLuint quadbuf[3];
	GLuint quadvao;
	GLuint shader;
	GLuint vertex_transform_location;
	GLuint mouse_location;
	GLuint aspect_ratio_location;
	GLuint texture0_location;
	GLuint img_handle;
};

ui_data ui_init();