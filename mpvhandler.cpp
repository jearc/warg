#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "mpvhandler.h"
#include "chat.h"

enum { playing, paused };
struct state {
	int pstatus;
	double time;
};

struct mpvhandler {
	mpv_handle *mpv;
	char *file;
	state s_canon;
	int64_t last_time;
};

mpvhandler *mpvh_create(char *uri)
{
	mpvhandler *h = (mpvhandler *)malloc(sizeof *h);

	h->mpv = mpv_create();
	mpv_initialize(h->mpv);
	mpv_set_option_string(h->mpv, "vo", "opengl-cb");
	mpv_set_option_string(h->mpv, "ytdl", "yes");
	
	h->file = strdup(uri);
	const char *cmd[] = { "loadfile", h->file, NULL };
	mpv_command(h->mpv, cmd);
	
	h->s_canon.pstatus = paused;
	h->s_canon.time = 0;
	
	h->last_time = SDL_GetPerformanceCounter();

	return h;
}

mpv_opengl_cb_context *mpvh_get_opengl_cb_api(mpvhandler *h)
{
	return (mpv_opengl_cb_context *)mpv_get_sub_api(h->mpv, MPV_SUB_API_OPENGL_CB);
}

void mpvh_update(mpvhandler *h)
{
	int64_t current_time = SDL_GetPerformanceCounter();
	double dt = (double)((current_time - h->last_time) /
	                     (double)SDL_GetPerformanceFrequency());
	if (h->s_canon.pstatus == playing)
		h->s_canon.time += dt;
	h->last_time = current_time;
	
	mpv_event *e;
	while (e = mpv_wait_event(h->mpv, 0), e->event_id != MPV_EVENT_NONE) {
		switch (e->event_id) {
		case MPV_EVENT_FILE_LOADED:
			mpv_command_string(h->mpv, "set pause yes");
			break;
		case MPV_EVENT_PLAYBACK_RESTART: {
			statusstr status = mpvh_statusstr(h);
			sendmsg(status.str);
			break;
		}
		default:
			break;
		}
	}
}

void mpvh_syncmpv(mpvhandler *h, state *s)
{
	mpv_set_property(h->mpv, "pause", MPV_FORMAT_FLAG, &s->pstatus);
	mpv_set_property(h->mpv, "time-pos", MPV_FORMAT_DOUBLE, &s->time);
}

void mpvh_pp(mpvhandler *h)
{
	h->s_canon.pstatus = h->s_canon.pstatus == playing ? paused : playing;
	mpvh_syncmpv(h, &h->s_canon);
}

void mpvh_seek(mpvhandler *h, double time)
{
	h->s_canon.time = time;
	mpvh_syncmpv(h, &h->s_canon);
}

void mpvh_seekrel(mpvhandler *h, double offset)
{
	h->s_canon.time += offset;
	mpvh_syncmpv(h, &h->s_canon);
}

statusstr mpvh_statusstr(mpvhandler *h)
{
	statusstr s;
	
	int64_t t, hh, mm, ss;
	t = round(h->s_canon.time);
	hh = t / 3600;
	mm = (t % 3600) / 60;
	ss = t % 60;
	
	snprintf(s.str, 50, "moov: %s %ld:%02ld:%02ld",
	         h->s_canon.pstatus == paused ? "paused" : "playing", hh, mm, ss);

	return s;
}
