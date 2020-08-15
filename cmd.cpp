#include <string.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "moov.h"

void cmd_pp(char *args, mpvhandler *mpvh);
void cmd_play(char *args, mpvhandler *mpvh);
void cmd_pause(char *args, mpvhandler *mpvh);
void cmd_status(char *args, mpvhandler *mpvh);
void cmd_seek(char *args, mpvhandler *mpvh);
void cmd_seekplus(char *args, mpvhandler *mpvh);
void cmd_seekminus(char *args, mpvhandler *mpvh);
void cmd_prev(char *args, mpvhandler *mpvh);
void cmd_next(char *args, mpvhandler *mpvh);
void cmd_index(char *args, mpvhandler *mpvh);
void cmd_set(char *args, mpvhandler *mpvh);

#define CMD(name, func)                                                        \
	{                                                                          \
		name, sizeof name - 1, func                                            \
	}
static struct {
	const char *name;
	size_t len;
	void (*func)(char *, mpvhandler *);
} cmdtab[] = { CMD("pp", cmd_pp), CMD("PLAY", cmd_play),
	CMD("PAUSE", cmd_pause), CMD("STATUS", cmd_status), CMD("SEEK", cmd_seek),
	CMD("SEEK+", cmd_seekplus), CMD("SEEK-", cmd_seekminus),
	CMD("PREV", cmd_prev), CMD("NEXT", cmd_next), CMD("INDEX", cmd_index),
	CMD("SET", cmd_set) };
size_t cmdcnt = sizeof cmdtab / sizeof cmdtab[0];

void handlecmd(char *text, mpvhandler *mpvh)
{
	while (*text && isspace(*text))
		text++;
	char *cmd = text;
	size_t cmdlen = 0;
	while (*text && !isspace(*text))
		text++, cmdlen++;
	while (*text && isspace(*text))
		text++;
	char *args = text;

	for (size_t i = 0; i < cmdcnt; i++) {
		int cmplen = max(cmdlen, cmdtab[i].len);
		if (strncmp(cmd, cmdtab[i].name, cmplen) == 0) {
			cmdtab[i].func(args, mpvh);
			break;
		}
	}
}

void cmd_pp(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	mpvh->pause(!mpvh->get_info().c_paused);
	mpvh->sendstatus();
}

void cmd_play(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	mpvh->pause(false);
	mpvh->sendstatus();
}

void cmd_pause(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	mpvh->pause(true);
	mpvh->sendstatus();
}

void cmd_status(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	mpvh->sendstatus();
}

void cmd_seek(char *args, mpvhandler *mpvh)
{
	mpvh->set_time(parsetime(args));
	mpvh->sendstatus();
}

void cmd_seekplus(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	mpvh->set_time(mpvh->get_info().c_time + parsetime(args));
	mpvh->sendstatus();
}

void cmd_seekminus(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	mpvh->set_time(mpvh->get_info().c_time - parsetime(args));
	mpvh->sendstatus();
}

void cmd_prev(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	mpvh->set_pl_pos(mpvh->get_info().pl_pos - 1);
	mpvh->sendstatus();
}

void cmd_next(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	mpvh->set_pl_pos(mpvh->get_info().pl_pos + 1);
	mpvh->sendstatus();
}

void cmd_index(char *args, mpvhandler *mpvh)
{
	mpvh->set_pl_pos(atoi(args) - 1);
	mpvh->sendstatus();
}

void cmd_set(char *args, mpvhandler *mpvh)
{
	uint64_t tok[5];
	for (size_t i = 0; i < 5; i++) {
		args = getint(args, &tok[i]);
		if (!args) {
			sendmsg("moov: bad cmd");
			return;
		}
	}
	mpvh->set_pl_pos(tok[0] - 1);
	mpvh->set_time(3600 * tok[1] + 60 * tok[2] + tok[3]);
	mpvh->pause(tok[4]);
	mpvh->sendstatus();
}
