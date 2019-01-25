struct mpvhandler;
struct mpv_opengl_cb_context;
struct statusstr { char str[50]; };
struct playstate {
	double time;
	int paused;
};
struct mpvinfo {
	char title[100];
	int track_curr;
	int track_cnt;
	double delay;
	double duration;
	playstate state;
	int audio_curr;
	int audio_cnt;
	int sub_curr;
	int sub_cnt;
	int muted;
	bool exploring;
	playstate explore_state;
};

mpvhandler *mpvh_create(char *uri);
mpv_opengl_cb_context *mpvh_get_opengl_cb_api(mpvhandler *h);
mpvinfo mpvh_getinfo(mpvhandler *h);
void mpvh_update(mpvhandler *h);
void mpvh_pp(mpvhandler *h);
void mpvh_seek(mpvhandler *h, double time);
void mpvh_seekrel(mpvhandler *h, double offset);
statusstr statestr(playstate st);
void mpvh_explore(mpvhandler *h);
void mpvh_explore_seek(mpvhandler *h, double time);
void mpvh_explore_accept(mpvhandler *h);
void mpvh_explore_cancel(mpvhandler *h);
void mpvh_toggle_mute(mpvhandler *h);
