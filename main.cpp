#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <mpv/opengl_cb.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <string>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_opengl3.h"
#include "moov.h"

#define MAX_MSG_LEN 1000

ImFont *pFont;

void *get_proc_address_mpv(void *fn_ctx, const char *name)
{
	UNUSED(fn_ctx);

	return SDL_GL_GetProcAddress(name);
}

void on_mpv_redraw(void *mpv_redraw)
{
	*(bool *)mpv_redraw = true;
}

void toggle_fullscreen(SDL_Window *win)
{
	bool fullscreen = SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN;
	SDL_SetWindowFullscreen(
		win, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
	SDL_ShowCursor(SDL_ENABLE);
}

void chatbox(std::vector<Message> &cl, bool scroll_to_bottom)
{
	auto chat_window_name = "chat_window";
	auto chat_window_flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoNav;
	auto chat_window_size = ImVec2(300, 300);
	auto chat_window_pos = ImVec2(300, 300);

	ImGui::SetNextWindowSize(chat_window_size);
	ImGui::SetNextWindowPos(chat_window_pos);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	bool display = true;
	ImGui::Begin(chat_window_name, &display, chat_window_flags);

	auto draw_list = ImGui::GetWindowDrawList();

	ImGui::PushFont(pFont);

	ImVec2 message_pos = chat_window_pos;
	for (auto &msg : cl) {
		auto text_size = ImGui::CalcTextSize(msg.text.c_str());
		ImVec2 rect_p_max(
			message_pos.x + text_size.x, message_pos.y + text_size.y);
		draw_list->AddRectFilled(message_pos, rect_p_max, msg.bg);
		draw_list->AddText(message_pos, msg.fg, msg.text.c_str());
		message_pos.y += text_size.y;
	}
	ImGui::PopFont();
	ImGui::End();
	ImGui::PopStyleVar(2);
}

bool readstdin(std::vector<Message> &cl, Player &mpvh)
{
	bool new_msg = false;

	static char buf[MAX_MSG_LEN];
	memset(buf, 0, sizeof buf);
	static size_t bufidx = 0;

	char c;
	while (read(0, &c, 1) != -1) {
		switch (c) {
		case EOF:
			break;
		case '\0': {
			cl.push_back({ buf, 0xff00ffff, 0x99000000 });
			//handlecmd(text, mpvh);
			new_msg = true;
			memset(buf, 0, sizeof buf);
			bufidx = 0;
			break;
		}
		default:
			bool significant = bufidx != 0 || !isspace(c);
			bool space_available = bufidx < (sizeof buf) - 1;
			if (significant && space_available)
				buf[bufidx++] = c;
			break;
		}
	}

	return new_msg;
}

void explorewin(Player &mpvh)
{
	PlayerInfo i = mpvh.get_info();

	static bool display = true;
	ImGui::Begin("Explore", &display, 0);

	if (ImGui::Button("Play/Pause"))
		mpvh.toggle_explore_paused();

	float progress = i.e_time / i.duration;
	ImGui::ProgressBar(progress, ImVec2(380, 20));
	if (ImGui::IsItemClicked() && i.duration) {
		auto min = ImGui::GetItemRectMin();
		auto max = ImGui::GetItemRectMax();
		auto mouse = ImGui::GetMousePos();

		float fraction = (mouse.x - min.x) / (max.x - min.x);
		double time = fraction * i.duration;

		mpvh.set_explore_time(time);
	}

	ImGui::Text(
		"%s", statestr(i.e_time, i.e_paused, i.pl_pos, i.pl_count).c_str());

	if (ImGui::Button("Accept"))
		mpvh.explore_accept();
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
		mpvh.explore_cancel();

	ImGui::End();
}

void dbgwin(SDL_Window *win, Player &mpvh)
{
	PlayerInfo i = mpvh.get_info();

	static bool display = true;
	ImGui::Begin("Debug", &display, 0);

	if (ImGui::Button("<"))
		sendmsg("PREV");
	ImGui::SameLine();
	ImGui::Text("T: %ld/%ld", i.pl_pos + 1, i.pl_count);
	ImGui::SameLine();
	if (ImGui::Button(">"))
		sendmsg("NEXT");

	ImGui::Text("%s", i.title.c_str());

	if (ImGui::Button("Play"))
		sendmsg("PLAY");
	ImGui::SameLine();
	if (ImGui::Button("Pause"))
		sendmsg("PAUSE");

	ImGui::Text(
		"%s", statestr(i.c_time, i.c_paused, i.pl_pos, i.pl_count).c_str());

	if (!i.exploring)
		ImGui::Text("Delay: %.f", i.delay);

	if (ImGui::Button("<"))
		mpvh.set_sub(i.sub_pos - 1);
	ImGui::SameLine();
	ImGui::Text("S: %ld/%ld", i.sub_pos, i.sub_count);
	ImGui::SameLine();
	if (ImGui::Button(">"))
		mpvh.set_sub(i.sub_pos + 1);

	if (ImGui::Button("<"))
		mpvh.set_audio(i.audio_pos - 1);
	ImGui::SameLine();
	ImGui::Text("A: %ld/%ld", i.audio_pos, i.audio_count);
	ImGui::SameLine();
	if (ImGui::Button(">"))
		mpvh.set_audio(i.audio_pos + 1);

	if (ImGui::Button("Explore"))
		mpvh.explore();
	ImGui::SameLine();
	ImGui::Text("Exploring: %d", i.exploring);
	if (i.exploring)
		explorewin(mpvh);

	if (ImGui::Button("Mute"))
		mpvh.toggle_mute();
	ImGui::SameLine();
	ImGui::Text("Muted: %d", i.muted);

	if (ImGui::Button("Fullscreen"))
		toggle_fullscreen(win);

	ImGui::End();
}

bool handle_sdl_events(SDL_Window *win)
{
	bool redraw = false;

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			exit(EXIT_SUCCESS);
		case SDL_KEYDOWN:
			switch (e.key.keysym.sym) {
			case SDLK_F11:
				toggle_fullscreen(win);
				break;
			default:
				ImGui_ImplSDL2_ProcessEvent(&e);
			}
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_EXPOSED)
				redraw = true;
			break;
		default:
			ImGui_ImplSDL2_ProcessEvent(&e);
		}
	}

	return redraw;
}

SDL_Window *init_window()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		die("SDL init failed");
	SDL_GL_SetAttribute(
		SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(
		SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_Window *window = SDL_CreateWindow("Moov", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, 1280, 720,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	SDL_GL_CreateContext(window);
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	glewInit();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsClassic();
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init("#version 150");

	ImGuiIO &io = ImGui::GetIO();
	pFont = io.Fonts->AddFontFromFileTTF(
		"/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Medium.ttf",
		40);

	return window;
}

int main(int argc, char **argv)
{
	int start_track = 0;
	double start_time = 0;
	int opt;
	while ((opt = getopt(argc, argv, "s:i:")) != -1) {
		switch (opt) {
		case 'i':
			start_track = atoi(optarg);
			break;
		case 's':
			start_time = parsetime(optarg);
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}
	int filec = argc - optind;
	char **filev = argv + optind;
	if (!filec)
		die("no files\n");

	fcntl(0, F_SETFL, O_NONBLOCK);

	SDL_Window *window = init_window();
	Player mpvh(filec, filev, start_track, start_time);
	mpv_opengl_cb_context *mpv_gl = mpvh.get_opengl_cb_api();
	mpv_opengl_cb_init_gl(mpv_gl, NULL, get_proc_address_mpv, NULL);

	bool mpv_redraw = false;
	mpv_opengl_cb_set_update_callback(mpv_gl, on_mpv_redraw, &mpv_redraw);

	std::vector<Message> cl;

	int64_t t_last = 0, t_now = 0;
	while (1) {
		SDL_Delay(10);
		t_now = SDL_GetPerformanceCounter();
		double delta = (t_now - t_last) / (double)SDL_GetPerformanceFrequency();
		if (delta <= 1 / 60.0)
			continue;
		t_last = t_now;

		bool scroll_to_bottom = readstdin(cl, mpvh);

		bool redraw = false;
		if (mpv_redraw) {
			redraw = true;
			mpv_redraw = false;
		}
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)
			redraw = true;

		if (handle_sdl_events(window))
			redraw = true;
		mpvh.update();

		//if (!redraw)
		//	continue;

		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		glClear(GL_COLOR_BUFFER_BIT);
		mpv_opengl_cb_draw(mpv_gl, 0, w, -h);
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);
		ImGui::NewFrame();
		chatbox(cl, scroll_to_bottom);
		dbgwin(window, mpvh);
		glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x,
			(int)ImGui::GetIO().DisplaySize.y);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	return 0;
}
