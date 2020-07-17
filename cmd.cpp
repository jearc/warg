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

#define CMD(name, func) { name, sizeof name - 1, func }
static struct {
	const char *name;
	size_t len;
	void (*func)(char *, mpvhandler *);
} cmdtab[] = {
	CMD("pp", cmd_pp),
	CMD("PLAY", cmd_play),
	CMD("PAUSE", cmd_pause),
	CMD("STATUS", cmd_status),
	CMD("SEEK", cmd_seek),
	CMD("SEEK+", cmd_seekplus),
	CMD("SEEK-", cmd_seekminus),
	CMD("PREV", cmd_prev),
	CMD("NEXT", cmd_next),
	CMD("INDEX", cmd_index),
	CMD("SET", cmd_set)
};
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
	player_set_paused(mpvh, !player_get_info(mpvh).c_paused);
	mpvh_sendstatus(mpvh);
}

void cmd_play(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	player_set_paused(mpvh, false);
	mpvh_sendstatus(mpvh);
}

void cmd_pause(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	player_set_paused(mpvh, true);
	mpvh_sendstatus(mpvh);
}

void cmd_status(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	mpvh_sendstatus(mpvh);
}

void cmd_seek(char *args, mpvhandler *mpvh)
{
	player_set_time(mpvh, parsetime(args));
	mpvh_sendstatus(mpvh);
}

void cmd_seekplus(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	player_set_time(mpvh, player_get_info(mpvh).c_time + parsetime(args));
	mpvh_sendstatus(mpvh);
}

void cmd_seekminus(char *args, mpvhandler *mpvh)
{
	double time = parsetime(args);
	player_set_time(mpvh, player_get_info(mpvh).c_time - parsetime(args));
	mpvh_sendstatus(mpvh);
}

void cmd_prev(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	player_set_pl_pos(mpvh, player_get_info(mpvh).pl_pos - 1);
	mpvh_sendstatus(mpvh);
}

void cmd_next(char *args, mpvhandler *mpvh)
{
	UNUSED(args);
	player_set_pl_pos(mpvh, player_get_info(mpvh).pl_pos + 1);
	mpvh_sendstatus(mpvh);
}

void cmd_index(char *args, mpvhandler *mpvh)
{
	player_set_pl_pos(mpvh, atoi(args) - 1);
	mpvh_sendstatus(mpvh);
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
	player_set_pl_pos(mpvh, tok[0] - 1);
	player_set_time(mpvh, 3600 * tok[1] + 60 * tok[2] + tok[3]);
	player_set_paused(mpvh, tok[4]);
	mpvh_sendstatus(mpvh);
}
