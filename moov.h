#define UNUSED(x) (void)(x)
#define max(a, b) ((a) > (b) ? (a) : (b))
// the timestr of 2^32 sec is "1193046:28:16"
#define TIMESTR_BUF_LEN 14
#define CHAT_BUFFER_SIZE 1000000
#define CHAT_MAX_MESSAGE_COUNT 10000
#define TITLE_STRING_LEN 100
#define STATUS_STRING_LEN 50

struct mpvhandler;
struct mpv_opengl_cb_context;

struct title_str { char str[TITLE_STRING_LEN]; };
struct statusstr { char str[STATUS_STRING_LEN]; };
struct timestr { char str[TIMESTR_BUF_LEN]; };

struct message {
	time_t time;
	size_t start, end;
	char *name, *text;
};
struct chatlog {
	char *buf;
	size_t next;
	message *msgs;
	size_t msgfirst, msgnext;
};

struct PlayerInfo {
	int64_t pl_pos, pl_count;
	int muted;
	
	title_str title;
	double duration;
	int64_t audio_pos, audio_count;
	int64_t sub_pos, sub_count;
	
	double c_time, delay;
	int c_paused;
	
	int exploring;
	double e_time;
	int e_paused;
};

void sendmsg(const char *fmt, ...);
chatlog init_chatlog();
char *logmsg(chatlog *cl, char *msg, size_t len);
int splitinput(char *buf, char **username, char **text);
void handlecmd(char *text, mpvhandler *mpvh);
double parsetime(char *str);
timestr sec_to_timestr(unsigned int seconds);
void die(const char *fmt, ...);
char *getint(char *str, uint64_t *n);

void mpvh_set_audio(mpvhandler *h, int64_t track);
void mpvh_set_sub(mpvhandler *h, int64_t track);
mpvhandler *mpvh_create(int filec, char **filev, int track, double time);
mpv_opengl_cb_context *mpvh_get_opengl_cb_api(mpvhandler *h);
void mpvh_update(mpvhandler *h);
statusstr statestr(double time, int paused, int64_t pl_pos, int64_t pl_count);
void mpvh_sendstatus(mpvhandler *h);
void mpvh_explore(mpvhandler *h);
void mpvh_explore_accept(mpvhandler *h);
void mpvh_explore_cancel(mpvhandler *h);
void mpvh_toggle_mute(mpvhandler *h);

PlayerInfo player_get_info(mpvhandler *p);
void player_set_paused(mpvhandler *h, int paused);
void player_set_time(mpvhandler *h, double time);
void player_set_pl_pos(mpvhandler *p, int64_t pl_pos);
void player_toggle_explore_paused(mpvhandler *h);
void player_set_explore_time(mpvhandler *h, double time);
