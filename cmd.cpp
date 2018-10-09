#include <mpv/client.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>

#include "util.h"
#include "cmd.h"
#include "chat.h"
#include "mpv.h"

void cmd_pp(char *args, mpv_handle *mpv);
void cmd_status(char *args, mpv_handle *mpv);
void cmd_seek(char *args, mpv_handle *mpv);
void cmd_seekplus(char *args, mpv_handle *mpv);
void cmd_seekminus(char *args, mpv_handle *mpv);
void cmd_index(char *args, mpv_handle *mpv);
void cmd_prev(char *args, mpv_handle *mpv);
void cmd_next(char *args, mpv_handle *mpv);

struct {
	const char *name;
	void (*fn)(char *, mpv_handle *);
} cmdtab[] = {
	{ "pp", cmd_pp },
	{ "STATUS", cmd_status },
	{ "SEEK", cmd_seek },
	{ "SEEK+", cmd_seekplus },
	{ "SEEK-", cmd_seekminus },
	{ "INDEX", cmd_index },
	{ "PREV", cmd_prev },
	{ "NEXT", cmd_next }
};

void cmd_pp(char *args, mpv_handle *mpv)
{
	UNUSED(args);

	mpv_command_string(mpv, "cycle pause");
	char statusbuf[50] = {};
	writestatus(mpv, statusbuf);
	sendmsg(statusbuf);
}

void cmd_status(char *args, mpv_handle *mpv)
{
	UNUSED(args);

	char statusbuf[50] = {};
	writestatus(mpv, statusbuf);
	sendmsg(statusbuf);
}

void cmd_seek(char *args, mpv_handle *mpv)
{
	double time = parsetime(args, strlen(args));
	mpv_set_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &time);
}

void cmd_seekplus(char *args, mpv_handle *mpv)
{
	double time = parsetime(args, strlen(args));
	double current_time;
	mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &current_time);
	double new_time = current_time + time;
	mpv_set_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &new_time);
}

void cmd_seekminus(char *args, mpv_handle *mpv)
{
	double time = parsetime(args, strlen(args));
	double current_time;
	mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &current_time);
	double new_time = current_time - time;
	mpv_set_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &new_time);
}

void cmd_index(char *args, mpv_handle *mpv)
{
	int64_t index = atoi(args) - 1;
	mpv_set_property(mpv, "playlist-pos", MPV_FORMAT_INT64, &index);
}

void cmd_prev(char *args, mpv_handle *mpv)
{
	UNUSED(args);

	mpv_command_string(mpv, "playlist-prev");
}

void cmd_next(char *args, mpv_handle *mpv)
{
	UNUSED(args);

	mpv_command_string(mpv, "playlist-next");
}

void handlecmd(char *text, mpv_handle *mpv)
{
	static size_t ncmd = sizeof cmdtab / sizeof cmdtab[0];

	while (isspace(*text))
		text++;

	size_t cmdlen = 0;
	while (text[cmdlen] && !isspace(text[cmdlen]))
		cmdlen++;

	char *args = text + cmdlen;
	while (*args != '\0' && isspace(*args))
		args++;

	for (size_t i = 0; i < ncmd; i++) {
		if (strstr(text, cmdtab[i].name) != text)
			continue;
		if (cmdlen != strlen(cmdtab[i].name))
			continue;
		cmdtab[i].fn(args, mpv);
		break;
	}
}