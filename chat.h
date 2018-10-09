struct Message {
	time_t time;
	char *from;
	char *text;
};

struct chatlog {
	Message *msg = NULL;
	size_t msg_max = 0;
	size_t msg_cnt = 0;
	pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
};

void sendmsg(const char *text);
void logmsg(chatlog *cl, char *username, char *text);
int splitinput(char *buf, char **username, char **text);