#define UNUSED(x) (void)(x)
#define max(a, b) ((a) > (b) ? (a) : (b))

struct mpvhandler;
struct mpv_opengl_cb_context;
struct statusstr {
	char str[50];
};
struct playstate {
	int64_t track;
	double time;
	int paused;
};
struct mpvinfo {
	char title[100];
	int64_t track_cnt;
	double delay;
	double duration;
	playstate state;
	int64_t audio_curr;
	int64_t audio_cnt;
	int64_t sub_curr;
	int64_t sub_cnt;
	int muted;
	bool exploring;
	playstate explore_state;
};
struct Message {
	time_t time;
	char *from;
	char *text;
};
struct chatlog {
	Message *msg = NULL;
	size_t msg_max = 0;
	size_t msg_cnt = 0;
};
struct timestr {
	char str[40];
};
struct ui_data {
	GLuint buf[2], vao, shader, vertex_transform_location, mouse_location;
};

void sendmsg(const char *fmt, ...);
void logmsg(chatlog *cl, char *username, char *text);
int splitinput(char *buf, char **username, char **text);
void handlecmd(char *text, mpvhandler *mpvh);
mpvhandler *mpvh_create(int filec, char **filev, int track, double time);
mpv_opengl_cb_context *mpvh_get_opengl_cb_api(mpvhandler *h);
mpvinfo mpvh_getinfo(mpvhandler *h);
void mpvh_update(mpvhandler *h);
void mpvh_set_state(mpvhandler *h, playstate s);
statusstr statestr(mpvinfo info, playstate st);
void mpvh_sendstatus(mpvhandler *h);
void mpvh_explore(mpvhandler *h);
void mpvh_explore_set_state(mpvhandler *h, playstate s);
void mpvh_explore_accept(mpvhandler *h);
void mpvh_explore_cancel(mpvhandler *h);
void mpvh_toggle_mute(mpvhandler *h);
double parsetime(char *str);
timestr sec_to_timestr(int seconds);
void die(const char *fmt, ...);
void mpvh_set_audio(mpvhandler *h, int64_t track);
void mpvh_set_sub(mpvhandler *h, int64_t track);
ui_data ui_init();