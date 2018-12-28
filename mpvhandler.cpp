#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mpvhandler.h"
#include "mpv.h"
#include "chat.h"

struct mpvhandler {
	mpv_handle *mpv;
};

mpvhandler *mpvh_create()
{
	mpvhandler *h = (mpvhandler *)malloc(sizeof *h);

	h->mpv = mpv_create();
	mpv_initialize(h->mpv);
	mpv_set_option_string(h->mpv, "vo", "opengl-cb");
	mpv_set_option_string(h->mpv, "ytdl", "yes");

	return h;
}

mpv_opengl_cb_context *mpvh_get_opengl_cb_api(mpvhandler *h)
{
	return (mpv_opengl_cb_context *)mpv_get_sub_api(h->mpv, MPV_SUB_API_OPENGL_CB);
}

void mpvh_loadfile(mpvhandler *h, char *uri)
{
	const char *cmd[] = { "loadfile", uri, NULL };
	mpv_command(h->mpv, cmd);
}

void mpvh_update(mpvhandler *h)
{
	mpv_event *e;
	while (e = mpv_wait_event(h->mpv, 0), e->event_id != MPV_EVENT_NONE) {
		switch (e->event_id) {
		case MPV_EVENT_FILE_LOADED:
			mpv_command_string(h->mpv, "set pause yes");
			break;
		case MPV_EVENT_PLAYBACK_RESTART: {
			char statusbuf[50] = {};
			writestatus(h->mpv, statusbuf);
			sendmsg(statusbuf);
			break;
		}
		default:
			break;
		}
	}
}

void mpvh_pp(mpvhandler *h)
{
	mpv_command_string(h->mpv, "cycle pause");
}

void mpvh_seek(mpvhandler *h, double time)
{
	mpv_set_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &time);
}

void mpvh_seekplus(mpvhandler *h, double time)
{
	double current_time;
	mpv_get_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &current_time);
	double new_time = current_time + time;
	mpv_set_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &new_time);
}

void mpvh_seekminus(mpvhandler *h, double time)
{
	double current_time;
	mpv_get_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &current_time);
	double new_time = current_time - time;
	mpv_set_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &new_time);
}

statusstr mpvh_statusstr(mpvhandler *h)
{
	statusstr s;

	double time;
	double duration;
	int64_t pos;
	int64_t count;
	bool paused;

	int64_t time_int, time_h, time_m, time_s;

	if (mpv_get_property(h->mpv, "playback-time", MPV_FORMAT_DOUBLE, &time))
		goto error;
	if (mpv_get_property(h->mpv, "duration", MPV_FORMAT_DOUBLE, &duration))
		goto error;
	if (mpv_get_property(h->mpv, "playlist-pos-1", MPV_FORMAT_INT64, &pos))
		goto error;
	if (mpv_get_property(h->mpv, "playlist-count", MPV_FORMAT_INT64, &count))
		goto error;
	if (mpv_get_property(h->mpv, "pause", MPV_FORMAT_FLAG, &paused))
		goto error;

	time_int = round(time);
	time_h = time_int / 3600;
	time_m = (time_int % 3600) / 60;
	time_s = time_int % 60;

	snprintf(s.str, 50, "moov: [%ld/%ld] %s %ld:%02ld:%02ld", pos, count,
		 paused ? "paused" : "playing", time_h, time_m, time_s);

	return s;

error:
	sprintf(s.str, "moov: not playing");
	return s;
}