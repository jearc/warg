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

void send_control(int64_t pos, double time, bool paused)
{
	uint8_t cmd = OUT_CONTROL;
	write(1, &cmd, 1);
	write(1, &pos, 8);
	write(1, &time, 8);
	write(1, &paused, 1);
}

bool handle_instruction(Player &p, std::vector<Message> &l)
{
	uint8_t cmd;
	if (read(0, &cmd, 1) != 1)
		return false;

	switch (cmd) {
	case IN_PAUSE: {
		bool paused;
		read(0, &paused, 1);
		p.pause(paused);
		break;
	}
	case IN_SEEK: {
		double time;
		read(0, &time, 8);
		p.set_time(time);
		break;
	}
	case IN_MESSAGE: {
		uint32_t fg, bg;
		read(0, &fg, 4);
		read(0, &bg, 4);
		uint32_t len;
		read(0, &len, 4);
		auto message = std::string(len + 1, '\0');
		read(0, &message[0], len);
		l.push_back({ message, fg, bg });
		break;
	}
	case IN_ADD_FILE: {
		uint32_t len;
		read(0, &len, 4);
		auto path = std::string(len + 1, '\0');
		read(0, &path[0], len);
		p.add_file(path.c_str());
		break;
	}
	case IN_SET_PLAYLIST_POSITION: {
		int64_t pos;
		read(0, &pos, 8);
		p.set_pl_pos(pos);
		break;
	}
	case IN_STATUS_REQUEST: {
		uint32_t request_id;
		read(0, &request_id, 4);
		uint8_t out_cmd = OUT_STATUS;
		write(1, &out_cmd, 1);
		write(1, &request_id, 4);
		auto info = p.get_info();
		write(1, &info.pl_pos, 8);
		write(1, &info.pl_count, 8);
		write(1, &info.c_time, 8);
		write(1, &info.c_paused, 1);
		break;
	}
	case IN_CLOSE:
		die("closed by ipc");
		break;
	default:
		die("invalid input stream state");
		break;
	}

	return true;
}

void inputwin()
{
	bool display = true;
	ImGui::Begin("Input", &display, 0);

	static char buf[1000] = { 0 };
	if (ImGui::InputText(
			"Input: ", buf, 1000, ImGuiInputTextFlags_EnterReturnsTrue)) {
		uint8_t cmd = OUT_USER_INPUT;
		write(1, &cmd, 1);
		uint32_t len = strlen(buf);
		write(1, &len, 4);
		write(1, buf, len);
		memset(buf, 0, 1000);
	}
	ImGui::End();
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

	if (ImGui::Button("<")) {
		auto info = mpvh.get_info();
		mpvh.set_pl_pos(info.pl_pos - 1);
		info = mpvh.get_info();
		send_control(info.pl_pos, info.c_time, info.c_paused);
	}
	ImGui::SameLine();
	ImGui::Text("T: %ld/%ld", i.pl_pos + 1, i.pl_count);
	ImGui::SameLine();
	if (ImGui::Button(">")) {
		auto info = mpvh.get_info();
		mpvh.set_pl_pos(info.pl_pos + 1);
		info = mpvh.get_info();
		send_control(info.pl_pos, info.c_time, info.c_paused);
	}

	ImGui::Text("%s", i.title.c_str());

	if (ImGui::Button("Play")) {
		mpvh.pause(false);
		auto info = mpvh.get_info();
		send_control(info.pl_pos, info.c_time, info.c_paused);
	}
	ImGui::SameLine();
	if (ImGui::Button("Pause")) {
		mpvh.pause(true);
		auto info = mpvh.get_info();
		send_control(info.pl_pos, info.c_time, info.c_paused);
	}

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
		20);

	return window;
}

int main(int argc, char **argv)
{
	fcntl(0, F_SETFL, O_NONBLOCK);

	SDL_Window *window = init_window();
	auto mpvh = Player();
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

		while (handle_instruction(mpvh, cl))
			;

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
		inputwin();
		chatbox(cl, false);
		dbgwin(window, mpvh);
		glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x,
			(int)ImGui::GetIO().DisplaySize.y);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
	}

	return 0;
}
