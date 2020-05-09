#include <string.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>

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

#define CMD(str) str, sizeof str - 1
static struct {
	const char *name;
	size_t len;
	void (*func)(char *, mpvhandler *);
} cmdtab[] = {
	{ CMD("pp"), cmd_pp },
	{ CMD("PLAY"), cmd_play },
	{ CMD("PAUSE"), cmd_pause },
	{ CMD("STATUS"), cmd_status },
	{ CMD("SEEK"), cmd_seek },
	{ CMD("SEEK+"), cmd_seekplus },
	{ CMD("SEEK-"), cmd_seekminus },
	{ CMD("PREV"), cmd_prev },
	{ CMD("NEXT"), cmd_next },
	{ CMD("INDEX"), cmd_index },
	{ CMD("SET"), cmd_set }
};
size_t cmdcnt = sizeof cmdtab / sizeof cmdtab[0];

void handlecmd(const char *t, mpvhandler *mpvh)
{
	char *text = (char *)t;
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

	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.paused = !i.state.paused;
	mpvh_set_state(mpvh, i.state);
	i = mpvh_getinfo(mpvh);
	mpvh_sendstatus(mpvh);
}

void cmd_play(char *args, mpvhandler *mpvh)
{
	UNUSED(args);

	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.paused = false;
	mpvh_set_state(mpvh, i.state);
	i = mpvh_getinfo(mpvh);
	mpvh_sendstatus(mpvh);
}

void cmd_pause(char *args, mpvhandler *mpvh)
{
	UNUSED(args);

	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.paused = true;
	mpvh_set_state(mpvh, i.state);
	i = mpvh_getinfo(mpvh);
	mpvh_sendstatus(mpvh);
}

void cmd_status(char *args, mpvhandler *mpvh)
{
	UNUSED(args);

	mpvh_sendstatus(mpvh);
}

void cmd_seek(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.time = time;
	mpvh_set_state(mpvh, i.state);
	mpvh_sendstatus(mpvh);
}

void cmd_seekplus(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.time += time;
	mpvh_set_state(mpvh, i.state);
	mpvh_sendstatus(mpvh);
}

void cmd_seekminus(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.time -= time;
	mpvh_set_state(mpvh, i.state);
	mpvh_sendstatus(mpvh);
}

void cmd_prev(char *args, mpvhandler *mpvh)
{
	UNUSED(args);

	mpvinfo i = mpvh_getinfo(mpvh);
	if (i.state.track - 1 < 0)
		return;
	playstate s = {};
	s.track = i.state.track - 1;
	s.paused = true;
	mpvh_set_state(mpvh, s);
	mpvh_sendstatus(mpvh);
}

void cmd_next(char *args, mpvhandler *mpvh)
{
	UNUSED(args);

	mpvinfo i = mpvh_getinfo(mpvh);
	if (i.state.track + 1 >= i.track_cnt)
		return;
	playstate s = {};
	s.track = i.state.track + 1;
	s.paused = true;
	mpvh_set_state(mpvh, s);
	mpvh_sendstatus(mpvh);
}

void cmd_index(char *args, mpvhandler *mpvh)
{
	mpvinfo i = mpvh_getinfo(mpvh);
	int track = atoi(args) - 1;
	if (!(0 <= track && track < i.track_cnt))
		return;
	playstate s = {};
	s.track = track;
	s.paused = true;
	mpvh_set_state(mpvh, s);
	mpvh_sendstatus(mpvh);
}

void cmd_set(char *args, mpvhandler *mpvh)
{
	long tok[5];
	for (size_t i = 0; i < 5; i++) {
		while (*args && !isdigit(*args))
			args++;
		if (!*args) {
			sendmsg("moov: bad cmd");
			return;
		}
		tok[i] = strtol(args, &args, 10);
		args++;
	}

	playstate s;
	s.track = tok[0] - 1;
	s.time += 3600 * tok[1];
	s.time += 60 * tok[2];
	s.time += tok[3];
	s.paused = tok[4];

	mpvh_set_state(mpvh, s);
	mpvh_sendstatus(mpvh);
}
