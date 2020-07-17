#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

#include "moov.h"

void sendmsg(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	fputc('\0', stdout);
	fflush(stdout);
}

chatlog init_chatlog()
{
	chatlog cl;
	cl.buf = (char *)calloc(CHAT_BUFFER_SIZE, 1);
	cl.msgs = (message *)calloc(CHAT_MAX_MESSAGE_COUNT, sizeof *cl.msgs);
	cl.next = 0;
	cl.msgfirst = 0;
	cl.msgnext = 0;
	return cl;
}

bool intersects(size_t a, size_t b, size_t x)
{
	return a <= b ? a <= x && x <= b : a <= x || x <= b;
}

void pop_between(chatlog *cl, size_t a, size_t b)
{
	while (cl->msgfirst != cl->msgnext &&
		intersects(a, b, cl->msgs[cl->msgfirst].start))
		cl->msgfirst++, cl->msgfirst %= CHAT_MAX_MESSAGE_COUNT;
}

char *logmsg(chatlog *cl, char *msg, size_t len)
{
	size_t to_end = CHAT_BUFFER_SIZE - cl->next;
	bool wraps = len + 1 > to_end;
	if (wraps) {
		pop_between(cl, cl->next, CHAT_BUFFER_SIZE);
		cl->next = 0;
	}

	char *text = NULL;
	message *msgnext = &cl->msgs[cl->msgnext];
	msgnext->start = cl->next;
	msgnext->name = &cl->buf[cl->next];
	for (size_t i = 0; msg[i] && i < CHAT_BUFFER_SIZE - 1; i++) {
		cl->buf[cl->next] = msg[i];
		if (!text && msg[i] == ':') {
			cl->buf[cl->next] = '\0';
			text = msgnext->text = &cl->buf[cl->next + 1];
		}
		cl->next++, cl->next %= CHAT_BUFFER_SIZE;
	}
	cl->buf[cl->next] = '\0';
	msgnext->end = cl->next;
	msgnext->time = time(0);
	cl->next++, cl->next %= CHAT_BUFFER_SIZE;

	pop_between(cl, msgnext->start, msgnext->end);
	cl->msgnext++, cl->msgnext %= CHAT_MAX_MESSAGE_COUNT;
	if (cl->msgfirst == cl->msgnext)
		cl->msgfirst++, cl->msgfirst %= CHAT_MAX_MESSAGE_COUNT;
	
	return text;
}
