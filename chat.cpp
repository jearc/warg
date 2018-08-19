#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <mpv/client.h>

#include "chat.h"
#include "cmd.h"

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

void sendmsg(const char *text)
{
	fputs(text, stdout);
	fputc('\0', stdout);
	fflush(stdout);
}

void logmsg(chatlog *cl, char *username, size_t username_len, char *text, size_t text_len)
{
	Message msg;
	time_t t = time(0);
	msg.time = t;
	msg.from = strndup(username, username_len);
	msg.text = strndup(text, text_len);

	pthread_mutex_lock(&cl->mtx);
	if (cl->msg_cnt >= cl->msg_max) {
		cl->msg_max += 100;
		cl->msg =
			(Message *)realloc(cl->msg, cl->msg_max * sizeof(Message));
	}
	cl->msg[cl->msg_cnt++] = msg;
	pthread_mutex_unlock(&cl->mtx);
}

int parsemsg(chatlog *cl, mpv_handle *mpv, char *buf, size_t len)
{
	char *colon = strchr(buf, ':');
	if (!colon)
		return 1;
	size_t username_len = colon - buf;
	if ((int)len < (colon - buf) + 1)
		return 1;
	char *text = colon + 1;
	size_t text_len = len - (text - buf);
	handlemsg(cl, mpv, buf, username_len, text, text_len);

	return 0;
}

void handlemsg(chatlog *cl, mpv_handle *mpv, char *username,
	size_t username_len, char *text, size_t text_len)
{
	logmsg(cl, username, username_len, text, text_len);
	parsecmd(text, mpv);
}