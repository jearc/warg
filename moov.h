#include <string>
#include <mpv/client.h>
#include <mpv/opengl_cb.h>

#define UNUSED(x) (void)(x)
#define max(a, b) ((a) > (b) ? (a) : (b))
// the timestr of 2^32 sec is "1193046:28:16"
#define TIMESTR_BUF_LEN 14
#define CHAT_BUFFER_SIZE 1000000
#define CHAT_MAX_MESSAGE_COUNT 10000
#define STATUS_STRING_LEN 50

struct statusstr {
	char str[STATUS_STRING_LEN];
};
struct timestr {
	char str[TIMESTR_BUF_LEN];
};

struct Message {
	std::string text;
	unsigned fg, bg;
};

struct PlayerInfo {
	int64_t pl_pos, pl_count;
	int muted;

	std::string title;
	double duration;
	int64_t audio_pos, audio_count;
	int64_t sub_pos, sub_count;

	double c_time, delay;
	int c_paused;

	int exploring;
	double e_time;
	int e_paused;
};

class Player {
public:
	void init(int filec, char **filev, int track, double time);
	void pause(int paused);
	void toggle_explore_paused();
	PlayerInfo get_info();
	void set_time(double time);
	void set_pl_pos(int64_t pl_pos);
	void set_explore_time(double time);
	void toggle_mute();
	void explore_cancel();
	void explore_accept();
	void explore();
	void sendstatus();
	void update();
	mpv_opengl_cb_context *get_opengl_cb_api();
	void set_audio(int64_t track);
	void set_sub(int64_t track);

private:
	void syncmpv();

	mpv_handle *mpv;
	int64_t last_time;
	int64_t c_pos;
	double c_time;
	int c_paused;
	int exploring;

	int64_t audio_count, sub_count;
	std::string title;
};

void sendmsg(const char *fmt, ...);
int splitinput(char *buf, char **username, char **text);
void handlecmd(char *text, Player *mpvh);
double parsetime(char *str);
timestr sec_to_timestr(unsigned int seconds);
void die(const char *fmt, ...);
char *getint(char *str, uint64_t *n);

statusstr statestr(double time, int paused, int64_t pl_pos, int64_t pl_count);
