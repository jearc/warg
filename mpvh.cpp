#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "moov.h"

struct mpvhandler {
	mpv_handle *mpv;
	char *file;
	int64_t last_time;
	mpvinfo info;
};

void mpvh_update_track_counts(mpvhandler *h);

mpvhandler *mpvh_create(char *uri)
{
	mpvhandler *h = (mpvhandler *)calloc(sizeof *h, 1);

	h->mpv = mpv_create();
	mpv_initialize(h->mpv);
	mpv_set_option_string(h->mpv, "vo", "opengl-cb");
	mpv_set_option_string(h->mpv, "ytdl", "yes");

	h->file = strdup(uri);
	const char *cmd[] = { "loadfile", h->file, NULL };
	mpv_command(h->mpv, cmd);

	h->last_time = mpv_get_time_us(h->mpv);

	h->info.track_curr = 0;
	h->info.track_cnt = 1;
	h->info.delay = 0;
	h->info.state.time = 0;
	h->info.state.paused = true;
	h->info.audio_curr = 1;
	h->info.audio_cnt = 1;
	h->info.sub_curr = 1;
	h->info.sub_cnt = 1;
	mpv_get_property(h->mpv, "ao-mute", MPV_FORMAT_FLAG, &h->info.muted);
	h->info.exploring = false;

	return h;
}

mpv_opengl_cb_context *mpvh_get_opengl_cb_api(mpvhandler *h)
{
	return (mpv_opengl_cb_context *)mpv_get_sub_api(
		h->mpv, MPV_SUB_API_OPENGL_CB);
}

void mpvh_syncmpv(mpvhandler *h)
{
	playstate *s =
		h->info.exploring ? &h->info.explore_state : &h->info.state;
	mpv_set_property(h->mpv, "pause", MPV_FORMAT_FLAG, &s->paused);
	double mpv_time;
	mpv_get_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &mpv_time);
	if (abs(mpv_time - s->time) < 5)
		return;
	mpv_set_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &s->time);
}

mpvinfo mpvh_getinfo(mpvhandler *h)
{
	return h->info;
}

void mpvh_update(mpvhandler *h)
{
	int64_t current_time = mpv_get_time_us(h->mpv);
	double dt = (double)(current_time - h->last_time) / 1000000;
	h->last_time = current_time;
	if (!h->info.state.paused)
		h->info.state.time += dt;
	if (h->info.exploring && !h->info.explore_state.paused)
		h->info.explore_state.time += dt;

	mpv_event *e;
	while (e = mpv_wait_event(h->mpv, 0), e->event_id != MPV_EVENT_NONE) {
		switch (e->event_id) {
		case MPV_EVENT_SHUTDOWN:
			break;
		case MPV_EVENT_LOG_MESSAGE:
			break;
		case MPV_EVENT_GET_PROPERTY_REPLY:
			break;
		case MPV_EVENT_SET_PROPERTY_REPLY:
			break;
		case MPV_EVENT_COMMAND_REPLY:
			break;
		case MPV_EVENT_START_FILE:
			break;
		case MPV_EVENT_END_FILE:
			break;
		case MPV_EVENT_FILE_LOADED: {
			mpvh_syncmpv(h);

			mpv_get_property(h->mpv, "duration", MPV_FORMAT_DOUBLE,
				&h->info.duration);

			char *title = NULL;
			mpv_get_property(h->mpv, "media-title",
				MPV_FORMAT_STRING, &title);
			strncpy(h->info.title, title, 99);
			mpv_free(title);

			mpvh_update_track_counts(h);

			mpv_get_property(h->mpv, "sub", MPV_FORMAT_INT64,
				&h->info.sub_curr);
			mpv_get_property(h->mpv, "audio", MPV_FORMAT_INT64,
				&h->info.audio_curr);
			break;
		}
		case MPV_EVENT_IDLE:
			break;
		case MPV_EVENT_TICK:
			break;
		case MPV_EVENT_CLIENT_MESSAGE:
			break;
		case MPV_EVENT_VIDEO_RECONFIG:
			break;
		case MPV_EVENT_SEEK:
			break;
		case MPV_EVENT_PLAYBACK_RESTART:
			break;
		case MPV_EVENT_PROPERTY_CHANGE:
			break;
		case MPV_EVENT_QUEUE_OVERFLOW:
			break;
		default:
			break;
		}
	}
}

void mpvh_set_state(mpvhandler *h, playstate s)
{
	h->info.state = s;
	if (!h->info.exploring)
		mpvh_syncmpv(h);
}

statusstr statestr(playstate st)
{
	statusstr s;
	timestr ts = sec_to_timestr(round(st.time));
	snprintf(s.str, 50, "%s %s", st.paused ? "paused" : "playing", ts.str);
	return s;
}

void mpvh_explore(mpvhandler *h)
{
	h->info.exploring = true;
	h->info.explore_state = h->info.state;
}

void mpvh_explore_set_state(mpvhandler *h, playstate s)
{
	h->info.explore_state = s;
	mpvh_syncmpv(h);
}

void mpvh_explore_accept(mpvhandler *h)
{
	h->info.exploring = false;
	h->info.state = h->info.explore_state;

	timestr ts = sec_to_timestr(round(h->info.state.time));
	sendmsg("SEEK %s", ts.str);
}

void mpvh_explore_cancel(mpvhandler *h)
{
	h->info.exploring = false;
	mpvh_syncmpv(h);
}

void mpvh_toggle_mute(mpvhandler *h)
{
	h->info.muted = !h->info.muted;
	mpv_set_property(h->mpv, "ao-mute", MPV_FORMAT_FLAG, &h->info.muted);
}

void mpvh_update_track_counts(mpvhandler *h)
{
	h->info.audio_cnt = h->info.sub_cnt = 0;

	int64_t cnt;
	mpv_get_property(h->mpv, "track-list/count", MPV_FORMAT_INT64, &cnt);

	for (int i = 0; i < cnt; i++) {
		char buf[100];
		snprintf(buf, 99, "track-list/%d/type", i);
		char *type = NULL;
		mpv_get_property(h->mpv, buf, MPV_FORMAT_STRING, &type);
		if (strcmp(type, "audio") == 0)
			h->info.audio_cnt++;
		if (strcmp(type, "sub") == 0)
			h->info.sub_cnt++;
		mpv_free(type);
	}
}