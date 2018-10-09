#include <mpv/client.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl_gl3.h"
#include "chat.h"

void draw_message(Message *msg)
{
	static int i = 0;
	char buf[20];
	snprintf(buf, 19, "msg%d", i);
	ImGui::PushStyleColor(ImGuiCol_ChildWindowBg,
			      ImVec4(0.0, 0.0, 0.f, 1.f));
	ImVec2 text_size = ImGui::CalcTextSize(msg->text);
	ImGui::BeginChild(buf, text_size);

	ImGui::Text("%s", msg->text);
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void draw_chatlog(chatlog *cl)
{
	ImVec2 size = ImVec2(300, 150);
	ImVec2 pos = ImVec2(10, 10);
	bool display = true;
	int chatlog_flags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::SetNextWindowPos(pos);
	ImGui::Begin("chatlog", &display, size, 0.0f, chatlog_flags);
	if (cl->msg_cnt > 0)
		draw_message(&cl->msg[0]);
	ImGui::End();
	ImGui::PopStyleVar();
}