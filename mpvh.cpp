#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "moov.h"

struct mpvhandler {
	mpv_handle *mpv;
	int64_t last_time;
	int64_t c_pos;
	double c_time;
	int c_paused;
	int exploring;
	struct {
		title_str title;
		int64_t audio_count, sub_count;
	} cache;
};

void mpvh_update_track_counts(mpvhandler *h);
void mpvh_syncmpv(mpvhandler *h);

void mpv_get_track_counts(mpv_handle *m, int64_t *audio, int64_t *sub)
{
	*audio = *sub = 0;
	int64_t count;
	mpv_get_property(m, "track-list/count", MPV_FORMAT_INT64, &count);
	for (int i = 0; i < count; i++) {
		char buf[100];
		snprintf(buf, 99, "track-list/%d/type", i);
		char *type;
		mpv_get_property(m, buf, MPV_FORMAT_STRING, &type);
		if (strcmp(type, "audio") == 0)
			(*audio)++;
		if (strcmp(type, "sub") == 0)
			(*sub)++;
		mpv_free(type);
	}
}

mpvhandler *mpvh_create(int filec, char **filev, int track, double time)
{
	mpvhandler *h = (mpvhandler *)malloc(sizeof *h);
	*h = (mpvhandler){0};

	h->mpv = mpv_create();
	mpv_initialize(h->mpv);
	mpv_set_option_string(h->mpv, "vo", "opengl-cb");
	mpv_set_option_string(h->mpv, "ytdl", "yes");

	for (int i = 0; i < filec; i++) {
		const char *cmd[] = { "loadfile", filev[i], "append", NULL };
		mpv_command(h->mpv, cmd);
	}

	h->last_time = mpv_get_time_us(h->mpv);
	h->c_pos = track;
	h->c_time = time;
	h->c_paused = true;
	h->exploring = false;
	
	mpvh_syncmpv(h);
	mpvh_sendstatus(h);

	return h;
}

mpv_opengl_cb_context *mpvh_get_opengl_cb_api(mpvhandler *h)
{
	return (mpv_opengl_cb_context *)mpv_get_sub_api(
		h->mpv, MPV_SUB_API_OPENGL_CB);
}

void mpvh_syncmpv(mpvhandler *p)
{
	int64_t mpv_pos;
	mpv_get_property(p->mpv, "playlist-pos", MPV_FORMAT_INT64, &mpv_pos);
	if (mpv_pos != p->c_pos) {
		mpv_set_property(p->mpv, "playlist-pos", MPV_FORMAT_INT64, &p->c_pos);
		p->exploring = false;
	}

	if (p->exploring)
		return;

	bool paused;
	mpv_get_property(p->mpv, "pause", MPV_FORMAT_FLAG, &paused);
	if (paused != p->c_paused)
		mpv_set_property(p->mpv, "pause", MPV_FORMAT_FLAG, &p->c_paused);

	double mpv_time;
	mpv_get_property(p->mpv, "time-pos", MPV_FORMAT_DOUBLE, &mpv_time);
	if (abs(mpv_time - p->c_time) > 5)
		mpv_set_property(p->mpv, "time-pos", MPV_FORMAT_DOUBLE, &p->c_time);
}

PlayerInfo player_get_info(mpvhandler *p)
{
	PlayerInfo i = {0};
	
	mpv_get_property(p->mpv, "playlist-pos", MPV_FORMAT_INT64, &i.pl_pos);
	mpv_get_property(p->mpv, "playlist-count", MPV_FORMAT_INT64, &i.pl_count);
	mpv_get_property(p->mpv, "muted", MPV_FORMAT_INT64, &i.muted);
	
	i.title = p->cache.title;

	mpv_get_property(p->mpv, "duration", MPV_FORMAT_DOUBLE, &i.duration);
	
	i.audio_count = p->cache.audio_count;
	i.sub_count = p->cache.sub_count;
	
	mpv_get_property(p->mpv, "audio", MPV_FORMAT_INT64, &i.audio_pos);
	mpv_get_property(p->mpv, "sub", MPV_FORMAT_INT64, &i.sub_pos);
	
	i.exploring = p->exploring;
	i.c_time = p->c_time;
	i.c_paused = p->c_paused;
	double mpv_time;
	mpv_get_property(p->mpv, "time-pos", MPV_FORMAT_DOUBLE, &mpv_time);
	if (!p->exploring) {
		i.delay = i.c_time - mpv_time;
	} else {
		i.e_time = mpv_time;
		mpv_get_property(p->mpv, "pause", MPV_FORMAT_FLAG, &i.e_paused);
	}
	
	return i;
}

void mpvh_update(mpvhandler *h)
{
	int64_t current_time = mpv_get_time_us(h->mpv);
	double dt = (double)(current_time - h->last_time) / 1000000;
	h->last_time = current_time;
	if (!h->c_paused)
		h->c_time += dt;

	mpv_event *e;
	while (e = mpv_wait_event(h->mpv, 0), e->event_id != MPV_EVENT_NONE) {
		switch (e->event_id) {
		case MPV_EVENT_SHUTDOWN: break;
		case MPV_EVENT_LOG_MESSAGE: break;
		case MPV_EVENT_GET_PROPERTY_REPLY: break;
		case MPV_EVENT_SET_PROPERTY_REPLY: break;
		case MPV_EVENT_COMMAND_REPLY: break;
		case MPV_EVENT_START_FILE: break;
		case MPV_EVENT_END_FILE: break;
		case MPV_EVENT_FILE_LOADED: {
			char *title;
			mpv_get_property(h->mpv, "media-title", MPV_FORMAT_STRING, &title);
			strncpy(h->cache.title.str, title, TITLE_STRING_LEN-1);
			h->cache.title.str[TITLE_STRING_LEN-1] = '\0';
			mpv_free(title);
			
			mpv_get_track_counts(h->mpv, &h->cache.audio_count,
				&h->cache.sub_count);
			
			mpvh_syncmpv(h);
			break;
		}
		case MPV_EVENT_IDLE: break;
		case MPV_EVENT_TICK: break;
		case MPV_EVENT_CLIENT_MESSAGE: break;
		case MPV_EVENT_VIDEO_RECONFIG: break;
		case MPV_EVENT_SEEK: break;
		case MPV_EVENT_PLAYBACK_RESTART:
			mpvh_syncmpv(h);
			break;
		case MPV_EVENT_PROPERTY_CHANGE: break;
		case MPV_EVENT_QUEUE_OVERFLOW: break;
		default: break;
		}
	}
}

statusstr statestr(double time, int paused, int64_t pl_pos, int64_t pl_count)
{
	statusstr s;
	timestr ts = sec_to_timestr(round(time));
	snprintf(s.str, 50, "%ld/%ld %s %s", pl_pos + 1, pl_count,
		paused ? "paused" : "playing", ts.str);
	return s;
}

void mpvh_sendstatus(mpvhandler *h)
{
	PlayerInfo i = player_get_info(h);
	sendmsg("moov: %s",
		statestr(i.c_time, i.c_paused, i.pl_pos, i.pl_count).str);
}

void mpvh_explore(mpvhandler *h)
{
	h->exploring = true;
}

void mpvh_explore_accept(mpvhandler *p)
{
	p->exploring = false;
	mpv_get_property(p->mpv, "time-pos", MPV_FORMAT_DOUBLE, &p->c_time);
	mpv_get_property(p->mpv, "pause", MPV_FORMAT_FLAG, &p->c_paused);
	timestr ts = sec_to_timestr(p->c_time);
	sendmsg("SET %ld %s %d", p->c_pos + 1, ts.str, p->c_paused);
}

void mpvh_explore_cancel(mpvhandler *h)
{
	h->exploring = false;
	mpvh_syncmpv(h);
}

void mpvh_toggle_mute(mpvhandler *p)
{
	int muted;
	mpv_get_property(p->mpv, "ao-mute", MPV_FORMAT_FLAG, &muted);
	muted = !muted;
	mpv_set_property(p->mpv, "ao-mute", MPV_FORMAT_FLAG, &muted);
}

void mpvh_set_audio(mpvhandler *h, int64_t track)
{
	mpv_set_property(h->mpv, "audio", MPV_FORMAT_INT64, &track);
}

void mpvh_set_sub(mpvhandler *h, int64_t track)
{
	mpv_set_property(h->mpv, "sub", MPV_FORMAT_INT64, &track);
}

void player_set_paused(mpvhandler *h, int paused)
{
	h->c_paused = paused;
	mpvh_syncmpv(h);
}

void player_set_time(mpvhandler *h, double time)
{
	h->c_time = time;
	mpvh_syncmpv(h);
}

void player_set_pl_pos(mpvhandler *p, int64_t pl_pos)
{
	p->c_pos = pl_pos;
	p->c_paused = true, p->c_time = 0;
	mpvh_syncmpv(p);
}

void player_toggle_explore_paused(mpvhandler *h)
{
	assert(h->exploring);
	int paused;
	mpv_get_property(h->mpv, "pause", MPV_FORMAT_FLAG, &paused);
	paused = !paused;
	mpv_set_property(h->mpv, "pause", MPV_FORMAT_FLAG, &paused);
}

void player_set_explore_time(mpvhandler *h, double time)
{
	assert(h->exploring);
	mpv_set_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &time);
}
