struct mpvhandler;
struct mpv_opengl_cb_context;
struct statusstr {
	char str[50];
};

mpvhandler *mpvh_create(char *uri);
mpv_opengl_cb_context *mpvh_get_opengl_cb_api(mpvhandler *h);
void mpvh_update(mpvhandler *h);
statusstr mpvh_statusstr(mpvhandler *h);
void mpvh_pp(mpvhandler *h);
void mpvh_seek(mpvhandler *h, double time);
void mpvh_seekrel(mpvhandler *h, double offset);
statusstr mpvh_mpvstatusstr(mpvhandler *h);