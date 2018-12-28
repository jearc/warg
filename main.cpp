#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <unistd.h>
#include <fcntl.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl_gl3.h"
#include "util.h"
#include "mpv.h"
#include "chat.h"
#include "cmd.h"
#include "ui.h"

#define MAX_MSG_LEN 1000

static Uint32 wakeup_on_mpv_events;
static Uint32 wakeup_on_mpv_redraw;
const char *PIPE = "/tmp/mpvpipe";
const uint16_t MARGIN_SIZE = 8;
const float DEFAULT_OPACITY = 0.7;
SDL_Window *WINDOW;
chatlog CHATLOG;
mpv_handle *MPV;
SDL_GLContext GLCONTEXT;
mpv_opengl_cb_context *MPV_GL;
ImVec4 CLEAR_COLOR = ImColor(114, 144, 154);
char CHAT_INPUT_BUF[256] = { 0 };
bool SCROLL_TO_BOTTOM = true;
int MOUSE_X = 0, MOUSE_Y = 0;
int LAST_MOUSE_MOVE = 0;
int64_t CURRENT_TICK = 0;
int64_t LAST_TICK = 0;
bool MOUSE_OVER_CONTROLS = false;
ImVec2 CHATSIZE = ImVec2(400, 200);
ImVec2 CHATPOS = ImVec2(-CHATSIZE.x - MARGIN_SIZE, -CHATSIZE.y - MARGIN_SIZE);
ImVec2 OSDSIZE = ImVec2(400, 78);
ImVec2 OSDPOS = ImVec2(MARGIN_SIZE, CHATPOS.y - MARGIN_SIZE - CHATSIZE.y);
float CHAT_OPACITY = DEFAULT_OPACITY;
float OSD_OPACITY = DEFAULT_OPACITY;
bool FILE_LOADED = false;
bool IS_PLAYLIST = false;
int LAST_COMPLETED_TRACK = -1;
bool REDRAW = 0;

void cleanup()
{
	if (MPV_GL)
		mpv_opengl_cb_uninit_gl(MPV_GL);
	if (MPV)
		mpv_terminate_destroy(MPV);
	SDL_GL_DeleteContext(GLCONTEXT);
	if (WINDOW)
		SDL_DestroyWindow(WINDOW);
}

static void die(const char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	cleanup();
	exit(1);
}

static void *get_proc_address_mpv(void *fn_ctx, const char *name)
{
	UNUSED(fn_ctx);

	return SDL_GL_GetProcAddress(name);
}

static void on_mpv_events(void *ctx)
{
	UNUSED(ctx);

	SDL_Event event;
	event.type = wakeup_on_mpv_events;
	SDL_PushEvent(&event);
}

static void on_mpv_redraw(void *ctx)
{
	UNUSED(ctx);

	SDL_Event event;
	event.type = wakeup_on_mpv_redraw;
	SDL_PushEvent(&event);
}

bool is_fullscreen()
{
	return SDL_GetWindowFlags(WINDOW) & SDL_WINDOW_FULLSCREEN;
}

void toggle_fullscreen()
{
	SDL_SetWindowFullscreen(
		WINDOW, is_fullscreen() ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
	SDL_ShowCursor(SDL_ENABLE);
}

void osd()
{
	if (!(MOUSE_OVER_CONTROLS || CURRENT_TICK - LAST_MOUSE_MOVE < 1000)) {
		if (is_fullscreen())
			SDL_ShowCursor(SDL_DISABLE);
		return;
	}
	SDL_ShowCursor(SDL_ENABLE);

	ImVec2 size = OSDSIZE;
	ImVec2 pos;
	pos.x = OSDPOS.x >= 0 ? OSDPOS.x :
				(int)ImGui::GetIO().DisplaySize.x + OSDPOS.x;
	pos.y = OSDPOS.y >= 0 ? OSDPOS.y :
				(int)ImGui::GetIO().DisplaySize.y + OSDPOS.y;
	bool display = true;
	ImGui::SetNextWindowPos(pos);
	ImGui::Begin("StatusDisplay", &display, size, OSD_OPACITY,
		     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			     ImGuiWindowFlags_NoMove |
			     ImGuiWindowFlags_NoScrollbar |
			     ImGuiWindowFlags_NoSavedSettings);
	auto filename = mpv_get_property_string(MPV, "media-title");
	ImGui::Text("%s", filename);
	char *pos_sec = mpv_get_property_string(MPV, "playback-time");
	char *total_sec = mpv_get_property_string(MPV, "duration");
	float progress = 0;
	if (pos_sec && total_sec) {
		float total_sec_f = strtof(total_sec, NULL);
		if (total_sec_f != 0)
			progress = strtof(pos_sec, NULL) / total_sec_f;
	}
	ImGui::ProgressBar(progress, ImVec2(380, 20));
	if (ImGui::IsItemClicked() && total_sec) {
		auto min = ImGui::GetItemRectMin();
		auto max = ImGui::GetItemRectMax();
		auto mouse = ImGui::GetMousePos();

		float fraction = (mouse.x - min.x) / (max.x - min.x);
		double time = fraction * strtof(total_sec, NULL);

		mpv_set_property(MPV, "time-pos", MPV_FORMAT_DOUBLE, &time);
	}
	if (ImGui::Button("PP"))
		mpv_command_string(MPV, "cycle pause");
	ImGui::SameLine();
	char *playback_time = mpv_get_property_osd_string(MPV, "playback-time");
	char *duration = mpv_get_property_osd_string(MPV, "duration");
	char *playlist_pos = mpv_get_property_osd_string(MPV, "playlist-pos-1");
	char *playlist_count =
		mpv_get_property_osd_string(MPV, "playlist-count");
	if (playback_time && duration) {
		char statusbuf[20];
		snprintf(statusbuf, 20, "%s / %s", playback_time, duration);
		ImGui::Text("%s", statusbuf);
	} else {
		ImGui::Text("Not playing");
	}
	ImGui::SameLine();
	if (ImGui::Button("<"))
		mpv_command_string(MPV, "playlist-prev");
	ImGui::SameLine();
	if (playlist_pos && playlist_count) {
		char statusbuf[20];
		snprintf(statusbuf, 20, "%s/%s", playlist_pos, playlist_count);
		ImGui::Text("%s", statusbuf);
	} else {
		ImGui::Text("0/0");
	}
	ImGui::SameLine();
	if (ImGui::Button(">"))
		mpv_command_string(MPV, "playlist-next");
	ImGui::SameLine();
	int naudio = 0;
	int nsubs = 0;
	get_num_audio_sub_tracks(MPV, &naudio, &nsubs);
	int64_t selsub = 0;
	mpv_get_property(MPV, "sub", MPV_FORMAT_INT64, &selsub);
	char subbuttonbuf[10] = { 0 };
	snprintf(subbuttonbuf, 10, "Sub: %ld/%d", selsub, nsubs);
	if (ImGui::Button(subbuttonbuf)) {
		mpv_command_string(MPV, "cycle sub");
	}
	ImGui::SameLine();
	int64_t selaudio = 0;
	mpv_get_property(MPV, "audio", MPV_FORMAT_INT64, &selaudio);
	char audiobuttonbuf[15] = { 0 };
	snprintf(audiobuttonbuf, 15, "Audio: %ld/%d", selaudio, naudio);
	ImGui::Button(audiobuttonbuf);
	ImGui::SameLine();
	if (ImGui::Button("Full")) {
		toggle_fullscreen();
	}
	MOUSE_OVER_CONTROLS = ImGui::IsWindowHovered();
	static bool mouse_down = false;
	static int last_pos_x = MOUSE_X, last_pos_y = MOUSE_Y;
	bool mouse_over_window =
		(MOUSE_X >= pos.x && MOUSE_X < pos.x + size.x) &&
		(MOUSE_Y >= pos.y && MOUSE_Y < pos.y + size.y);
	if (ImGui::IsMouseClicked(0) && mouse_over_window) {
		mouse_down = true;
	}
	if (mouse_down) {
		int delta_x = MOUSE_X - last_pos_x;
		int delta_y = MOUSE_Y - last_pos_y;
		OSDPOS.x += delta_x;
		OSDPOS.y += delta_y;
	}
	if (ImGui::IsMouseReleased(0)) {
		mouse_down = false;
	}
	last_pos_x = MOUSE_X;
	last_pos_y = MOUSE_Y;
	ImGui::End();
}

void chatbox()
{
	ImVec2 size = CHATSIZE;
	ImVec2 pos;
	pos.x = CHATPOS.x >= 0 ? CHATPOS.x :
				 (int)ImGui::GetIO().DisplaySize.x + CHATPOS.x;
	pos.y = CHATPOS.y >= 0 ? CHATPOS.y :
				 (int)ImGui::GetIO().DisplaySize.y + CHATPOS.y;

	bool display = true;
	ImGui::SetNextWindowSize(size, ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowPos(pos);
	ImGui::Begin("ChatBox", &display, size, CHAT_OPACITY,
		     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			     ImGuiWindowFlags_NoMove |
			     ImGuiWindowFlags_NoScrollbar |
			     ImGuiWindowFlags_NoSavedSettings);
	ImGui::BeginChild("LogRegion",
			  ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()),
			  false, ImGuiWindowFlags_NoScrollbar);
	for (size_t i = 0; i < CHATLOG.msg_cnt; i++) {
		Message *msg = &CHATLOG.msg[i];
		tm *now = localtime(&msg->time);
		char buf[20] = { 0 };
		strftime(buf, sizeof(buf), "%X", now);
		ImGui::TextWrapped("[%s] %s: %s", buf, msg->from, msg->text);
	}
	if (SCROLL_TO_BOTTOM) {
		ImGui::SetScrollHere();
		SCROLL_TO_BOTTOM = false;
	}
	ImGui::EndChild();
	ImGui::PushItemWidth(size.x - 8 * 2);
	if (ImGui::InputText("", CHAT_INPUT_BUF, 256,
			     ImGuiInputTextFlags_EnterReturnsTrue, NULL,
			     NULL) &&
	    strlen(CHAT_INPUT_BUF)) {
		sendmsg(CHAT_INPUT_BUF);
		strcpy(CHAT_INPUT_BUF, "");
	}
	static bool mouse_down = false;
	static int last_pos_x = MOUSE_X, last_pos_y = MOUSE_Y;
	bool mouse_over_window =
		(MOUSE_X >= pos.x && MOUSE_X < pos.x + size.x) &&
		(MOUSE_Y >= pos.y && MOUSE_Y < pos.y + size.y);
	if (ImGui::IsMouseClicked(0) && mouse_over_window) {
		mouse_down = true;
	}
	if (mouse_down) {
		int delta_x = MOUSE_X - last_pos_x;
		int delta_y = MOUSE_Y - last_pos_y;
		CHATPOS.x += delta_x;
		CHATPOS.y += delta_y;
	}
	if (ImGui::IsMouseReleased(0)) {
		mouse_down = false;
	}
	last_pos_x = MOUSE_X;
	last_pos_y = MOUSE_Y;
	ImGui::PopItemWidth();
	if (ImGui::IsItemHovered() ||
	    (ImGui::IsRootWindowOrAnyChildFocused() &&
	     !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
		ImGui::SetKeyboardFocusHere(-1);
	ImGui::End();
}

void ui()
{
	ImGui_ImplSdlGL3_NewFrame(WINDOW);

	osd();
	chatbox();
	//	draw_chatlog(&CHATLOG);

	glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x,
		   (int)ImGui::GetIO().DisplaySize.y);
	ImGui::Render();
}

void readstdin()
{
	static char buf[MAX_MSG_LEN];
	memset(buf, 0, sizeof buf);
	size_t bufidx = 0;
	
	char c;
	long res;
	while (read(0, &c, 1) != -1) {
		switch (c) {
		case EOF:
			break;
		case '\0':
			char *username, *text;
			splitinput(buf, &username, &text);
			logmsg(&CHATLOG, username, text);
			handlecmd(text, MPV);
			SCROLL_TO_BOTTOM = true;
			memset(buf, 0, sizeof buf);
			bufidx = 0;
			break;
		default:
			bool significant = bufidx != 0 || !isspace(c);
			bool space_available = bufidx < (sizeof buf) - 1;
			if (significant && space_available)
				buf[bufidx++] = c;
			break;
		}
	}
}			

char *playlistpath()
{
	char *path;
	int err = asprintf(&path, "%s/.moov_playlist", getenv("HOME"));
	if (err == -1)
		die("asprintf failed");

	return path;
}

char *trackpath()
{
	char *path;
	int err = asprintf(&path, "%s/.moov_track", getenv("HOME"));
	if (err == -1)
		die("asprintf failed");

	return path;
}

void load_playlist(int *last_completed_track)
{
	int size;
	size_t n;
	char *path, *line;
	FILE *fp;

	path = playlistpath();
	fp = fopen(path, "r");
	if (!fp)
		die("could not load playlist file.");
	line = NULL;
	size = 0, n = 0;
	while ((size = getline(&line, &n, fp)) != -1) {
		if (line[size - 1] == '\n')
			line[size - 1] = '\0';
		const char *cmd[] = { "loadfile", line, "append", NULL };
		mpv_command(MPV, cmd);
		free(line);
		line = NULL;
	}
	fclose(fp);
	free(path);

	path = trackpath();
	fp = fopen(path, "r");
	if (!fp)
		die("could not load track file.");
	line = NULL;
	n = 0;
	size = getline(&line, &n, fp);
	errno = 0;
	int64_t track = strtol(line, NULL, 10);
	if (errno)
		die("could not parse track file.");
	*last_completed_track = track;
	track++;
	mpv_set_property(MPV, "playlist-pos", MPV_FORMAT_INT64, &track);
	fclose(fp);
	free(path);
}

void save_track(int n)
{
	char *path = trackpath();
	FILE *fp = fopen(path, "w");
	fprintf(fp, "%d\n", n);
	fclose(fp);
	free(path);
}

void save_playlist(int numfiles, char **files)
{
	char *path = playlistpath();
	FILE *fp = fopen(path, "w");
	for (int i = 0; i < numfiles; i++)
		fprintf(fp, "%s\n", files[i]);
	fclose(fp);
	free(path);
}

void handle_keydown(SDL_Event event)
{
	if (event.key.keysym.sym == SDLK_F11)
		toggle_fullscreen();
	else if (event.key.keysym.mod & KMOD_CTRL &&
		 event.key.keysym.sym == SDLK_SPACE)
		mpv_command_string(MPV, "cycle pause");
	else
		ImGui_ImplSdlGL3_ProcessEvent(&event);
}

void handle_mpv_events(bool *first_load, double seekto)
{
	while (1) {
		mpv_event *mp_event = mpv_wait_event(MPV, 0);
		if (mp_event->event_id == MPV_EVENT_NONE) {
			break;
		} else if (mp_event->event_id == MPV_EVENT_FILE_LOADED) {
			mpv_command_string(MPV, "set pause yes");
		} else if (mp_event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
			FILE_LOADED = true;
			if (*first_load && seekto) {
				mpv_set_property(MPV, "time-pos",
						 MPV_FORMAT_DOUBLE, &seekto);
				*first_load = false;
			} else {
				char statusbuf[50] = {};
				writestatus(MPV, statusbuf);
				sendmsg(statusbuf);
			}
		} else if (mp_event->event_id == MPV_EVENT_END_FILE &&
			   !FILE_LOADED) {
			sendmsg("moov: m̛̿̇al͒f̃un̩cͯt̿io̲n̙͌͢");
			die("couldn't load file");
		}
	}
}

void handle_events(bool *first_load, double seekto)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			cleanup();
			exit(0);
		} else if (event.type == SDL_KEYDOWN) {
			handle_keydown(event);
		} else if (event.type == SDL_WINDOWEVENT &&
		           event.window.event == SDL_WINDOWEVENT_EXPOSED) {
			REDRAW = true;
		} else if (event.type == wakeup_on_mpv_redraw) {
			REDRAW = true;
		} else if (event.type == wakeup_on_mpv_events) {
			handle_mpv_events(first_load, seekto);
		} else {
			ImGui_ImplSdlGL3_ProcessEvent(&event);
		}
	}
}

void save_playlist_state()
{
	int64_t playlist_pos;
	mpv_get_property(MPV, "playlist-pos", MPV_FORMAT_INT64, &playlist_pos);
	if (!IS_PLAYLIST || playlist_pos <= LAST_COMPLETED_TRACK)
		return;
	char *pos_sec = mpv_get_property_string(MPV, "playback-time");
	char *total_sec = mpv_get_property_string(MPV, "duration");
	float progress = 0;
	if (pos_sec && total_sec) {
		float total_sec_f = strtof(total_sec, NULL);
		if (total_sec_f != 0)
			progress = strtof(pos_sec, NULL) / total_sec_f;
	}
	if (progress > 0.8) {
		LAST_COMPLETED_TRACK = playlist_pos;
		save_track(playlist_pos);
	}
}

struct argconf {
	double seekto = 0.0;

	bool resume = false;

	char *uri[100] = {};
	size_t uri_cnt = 0;
};

argconf parseargs(int argc, char *argv[])
{
	argconf conf;

	if (argc <= 1)
		return conf;

	bool expecting_seekto = false;
	for (int i = 1; i < argc; i++) {
		fprintf(stderr, "arg: %s\n", argv[i]);
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'r':
				conf.resume = true;
				break;
			case 's':
				expecting_seekto = true;
				break;
			default:
				die("invalid arg");
				break;
			}
		} else if (expecting_seekto) {
			conf.seekto = parsetime(argv[i], strlen(argv[i]));
		} else if (conf.uri_cnt < 100) {
			conf.uri[conf.uri_cnt++] = argv[i];
		}
	}

	return conf;
}

int main(int argc, char *argv[])
{
	argconf conf = parseargs(argc, argv);
	
	fcntl(0, F_SETFL, O_NONBLOCK);

	MPV = mpv_create();
	if (!MPV)
		die("context init failed");

	if (mpv_initialize(MPV) < 0)
		die("mpv init failed");

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		die("SDL init failed");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
			    SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			    SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	WINDOW = SDL_CreateWindow("Moov", SDL_WINDOWPOS_CENTERED,
				  SDL_WINDOWPOS_CENTERED, 1280, 720,
				  SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN |
					  SDL_WINDOW_RESIZABLE);
	if (!WINDOW)
		die("failed to create SDL window");

	MPV_GL = (mpv_opengl_cb_context *)mpv_get_sub_api(
		MPV, MPV_SUB_API_OPENGL_CB);
	if (!MPV_GL)
		die("failed to create mpv GL API handle");

	GLCONTEXT = SDL_GL_CreateContext(WINDOW);
	if (!GLCONTEXT)
		die("failed to create SDL GL context");

	glewInit();

	ImGui_ImplSdlGL3_Init(WINDOW);

	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF(
		"/usr/local/share/moov/liberation_sans.ttf", 14.0f);

	if (mpv_opengl_cb_init_gl(MPV_GL, NULL, get_proc_address_mpv, NULL) < 0)
		die("failed to initialize mpv GL context");

	if (mpv_set_option_string(MPV, "vo", "opengl-cb") < 0)
		die("failed to set VO");

	mpv_set_option_string(MPV, "ytdl", "yes");
	//	mpv_set_option_string(MPV, "ytdl-raw-options",
	//			      "format=\"bestvideo[height<="
	//			      "720][ext=mp4]+bestaudio/"
	//			      "best[height<=720]/best\"");
	mpv_set_option_string(MPV, "input-ipc-server", PIPE);

	wakeup_on_mpv_events = SDL_RegisterEvents(1);
	wakeup_on_mpv_redraw = SDL_RegisterEvents(1);
	
	if (wakeup_on_mpv_events == (Uint32)-1 ||
	    wakeup_on_mpv_redraw == (Uint32)-1)
		die("could not register events");
	mpv_set_wakeup_callback(MPV, on_mpv_events, NULL);
	mpv_opengl_cb_set_update_callback(MPV_GL, on_mpv_redraw, NULL);

	if (conf.resume) {
		IS_PLAYLIST = true;
		load_playlist(&LAST_COMPLETED_TRACK);
		FILE_LOADED = true;
	} else if (conf.uri_cnt > 0) {
		const char *cmd[] = { "loadfile", conf.uri[0], NULL };
		mpv_command(MPV, cmd);
		for (size_t i = 1; i < conf.uri_cnt; i++) {
			const char *cmd2[] = { "loadfile", conf.uri[i],
					       "append", NULL };
			mpv_command(MPV, cmd2);
		}
		int64_t playlist_count;
		mpv_get_property(MPV, "playlist-count", MPV_FORMAT_INT64,
				 &playlist_count);
		if (playlist_count > 1) {
			save_playlist(conf.uri_cnt, conf.uri);
			save_track(-1);
			IS_PLAYLIST = true;
		}
	} else {
		die("no uris");
	}

	bool first_load = true;
	while (1) {
		SDL_Delay(10);
		CURRENT_TICK = SDL_GetTicks();
		if (CURRENT_TICK - LAST_TICK <= 16)
			continue;
		LAST_TICK = CURRENT_TICK;
		int old_mouse_x = MOUSE_X, old_mouse_y = MOUSE_Y;
		SDL_GetMouseState(&MOUSE_X, &MOUSE_Y);
		if (old_mouse_x != MOUSE_X || old_mouse_y != MOUSE_Y)
			LAST_MOUSE_MOVE = CURRENT_TICK;
			
		readstdin();
		
		REDRAW = 0;

		handle_events(&first_load, conf.seekto);

		int w, h;
		SDL_GetWindowSize(WINDOW, &w, &h);
		glClearColor(CLEAR_COLOR.x, CLEAR_COLOR.y, CLEAR_COLOR.z,
			     CLEAR_COLOR.w);
		glClear(GL_COLOR_BUFFER_BIT);
		if (REDRAW)
			mpv_opengl_cb_draw(MPV_GL, 0, w, -h);
		ui();
		SDL_GL_SwapWindow(WINDOW);

		save_playlist_state();
	}

	cleanup();

	return 0;
}
