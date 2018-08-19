#include <mpv/client.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "util.h"
#include "cmd.h"
#include "chat.h"
#include "mpv.h"

void cmd_pp(char *args, mpv_handle *mpv);
void cmd_status(char *args, mpv_handle *mpv);
void cmd_seek(char *args, mpv_handle *mpv);
void cmd_seek_plus(char *args, mpv_handle *mpv);
void cmd_seek_minus(char *args, mpv_handle *mpv);
void cmd_index(char *args, mpv_handle *mpv);
void cmd_prev(char *args, mpv_handle *mpv);
void cmd_next(char *args, mpv_handle *mpv);

struct {
	const char *name;
	size_t namelen;
	void (*func)(char *, mpv_handle *);
} CMDTAB[] = {
	{ "pp", strlen("pp"), cmd_pp },
	{ "STATUS", strlen("STATUS"), cmd_status },
	{ "SEEK", strlen("SEEK"), cmd_seek },
	{ "SEEK+", strlen("SEEK+"), cmd_seek_plus },
	{ "SEEK-", strlen("SEEK-"), cmd_seek_minus },
	{ "INDEX", strlen("INDEX"), cmd_index },
	{ "PREV", strlen("PREV"), cmd_prev },
	{ "NEXT", strlen("NEXT"), cmd_next }
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

void cmd_seek_plus(char *args, mpv_handle *mpv)
{
	double time = parsetime(args, strlen(args));
	double current_time;
	mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &current_time);
	double new_time = current_time + time;
	mpv_set_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &new_time);
}

void cmd_seek_minus(char *args, mpv_handle *mpv)
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

void parsecmd(char *text, mpv_handle *mpv)
{
	static size_t cmdcnt = sizeof CMDTAB / sizeof CMDTAB[0];

	while (isspace(*text)) text++;

	size_t cmdlen = 0;
	while (text[cmdlen] != '\0' && !isspace(text[cmdlen])) cmdlen++;

	char *args = text + cmdlen;
	while (*args != '\0' && isspace(*args)) args++;

	for (size_t i = 0; i < cmdcnt; i++) {
		if (cmdlen != CMDTAB[i].namelen)
			continue;
		if (strstr(text, CMDTAB[i].name) != text)
			continue;
		return CMDTAB[i].func(args, mpv);
	}
}