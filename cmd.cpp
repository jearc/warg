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
};
size_t cmdcnt = sizeof cmdtab / sizeof cmdtab[0];

void handlecmd(char *text, mpvhandler *mpvh)
{
	char *cmd = text;
	while (*cmd && isspace(*cmd))
		cmd++;
	char *cmdend = cmd;
	while (*cmdend && !isspace(*cmdend))
		cmdend++;
	char *args = cmdend;
	while (*args && isspace(*args))
		args++;
	
	size_t cmdlen = cmdend - cmd;
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
	statusstr status = statestr(i.state);
	sendmsg(status.str);
}

void cmd_play(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	
	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.paused = false;
	mpvh_set_state(mpvh, i.state);
	i = mpvh_getinfo(mpvh);
	statusstr status = statestr(i.state);
	sendmsg(status.str);
}

void cmd_pause(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	
	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.paused = true;
	mpvh_set_state(mpvh, i.state);
	i = mpvh_getinfo(mpvh);
	statusstr status = statestr(i.state);
	sendmsg(status.str);
}

void cmd_status(char *args, mpvhandler *mpvh)
{
	UNUSED(args);

	mpvinfo i = mpvh_getinfo(mpvh);
	statusstr status = statestr(i.state);
	sendmsg(status.str);
}

void cmd_seek(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.time = time;
	mpvh_set_state(mpvh, i.state);
}

void cmd_seekplus(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.time += time;
	mpvh_set_state(mpvh, i.state);
}

void cmd_seekminus(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(mpvh);
	i.state.time -= time;
	mpvh_set_state(mpvh, i.state);
}
