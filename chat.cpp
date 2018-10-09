#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <mpv/client.h>
#include <ctype.h>

#include "chat.h"
#include "cmd.h"

void sendmsg(const char *text)
{
	fputs(text, stdout);
	fputc('\0', stdout);
	fflush(stdout);
}

void logmsg(chatlog *cl, char *username, char *text)
{
	Message msg;
	time_t t = time(0);
	msg.time = t;
	msg.from = strdup(username);
	msg.text = strdup(text);

	pthread_mutex_lock(&cl->mtx);
	if (cl->msg_cnt >= cl->msg_max) {
		cl->msg_max += 100;
		cl->msg = (Message *)realloc(cl->msg,
					     cl->msg_max * sizeof(*cl->msg));
	}
	cl->msg[cl->msg_cnt++] = msg;
	pthread_mutex_unlock(&cl->mtx);
}

int splitinput(char *buf, char **username, char **msg)
{
	*username = buf;
	char *colon = strchr(buf, ':');
	if (!colon)
		return 1;
	*colon = '\0';
	*msg = colon + 1;
	return 0;
}