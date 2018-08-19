#include <mpv/client.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

void writestatus(mpv_handle *mpv, char *buf)
{
        double time;
        double duration;
        int64_t pos;
        int64_t count;
        bool paused;

        int64_t time_int, time_h, time_m, time_s;

        if (mpv_get_property(mpv, "playback-time", MPV_FORMAT_DOUBLE, &time))
                goto error;
        if (mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &duration))
                goto error;
        if (mpv_get_property(mpv, "playlist-pos-1", MPV_FORMAT_INT64, &pos))
                goto error;
        if (mpv_get_property(mpv, "playlist-count", MPV_FORMAT_INT64, &count))
                goto error;
        if (mpv_get_property(mpv, "pause", MPV_FORMAT_FLAG, &paused))
                goto error;
        
        time_int = round(time);
        time_h = time_int / 3600;
        time_m = (time_int % 3600) / 60;
        time_s = time_int % 60;

        snprintf(buf, 50, "moov: [%ld/%ld] %s %ld:%02ld:%02ld", pos, count,
                paused ? "paused" : "playing", time_h, time_m, time_s);

        return;

error:
        sprintf(buf, "moov: not playing");
}

int get_num_audio_sub_tracks(mpv_handle *mpv, int *naudio, int *nsubs)
{
        int64_t ntracks;
	if (mpv_get_property(mpv, "track-list/count", MPV_FORMAT_INT64,
			     &ntracks))
		return -1;
	int naudio_ = 0, nsubs_ = 0;
	for (int i = 0; i < ntracks; i++) {
		char propbuf[100];
		snprintf(propbuf, 100, "track-list/%d/type", i);
		char *type = NULL;
		if (mpv_get_property(mpv, propbuf, MPV_FORMAT_STRING, &type))
			return -2;
		if (strcmp(type, "sub") == 0)
			nsubs_++;
		if (strcmp(type, "audio") == 0)
			naudio_++;
	}

	*naudio = naudio_;
	*nsubs = nsubs_;
	return 0;
}