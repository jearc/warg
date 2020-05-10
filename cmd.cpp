#include <string.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <map>
#include <functional>
#include <string>
#include <iostream>
#include <regex>
#include <sys/types.h>
#include <regex.h>

#include "moov.h"

void cmd_status(std::string_view args, mpvhandler &mpvh);
void cmd_pp(std::string_view args, mpvhandler &mpvh);
void cmd_play(std::string_view args, mpvhandler &mpvh);
void cmd_pause(std::string_view args, mpvhandler &mpvh);
void cmd_seek(std::string_view args, mpvhandler &mpvh);
void cmd_seekplus(std::string_view args, mpvhandler &mpvh);
void cmd_seekminus(std::string_view args, mpvhandler &mpvh);
void cmd_prev(std::string_view args, mpvhandler &mpvh);
void cmd_next(std::string_view args, mpvhandler &mpvh);
void cmd_index(std::string_view args, mpvhandler &mpvh);
void cmd_set(std::string_view args, mpvhandler &mpvh);

typedef std::function<void(std::string_view, mpvhandler &)> Command;
std::map<std::string, Command, std::less<>> cmd_tab = {
	{"STATUS", cmd_status},
	{"pp", cmd_pp},
	{"PLAY", cmd_play},
	{"PAUSE", cmd_pause},
	{"SEEK", cmd_seek},
	{"SEEK+", cmd_seekplus},
	{"SEEK-", cmd_seekminus},
	{"PREV", cmd_prev},
	{"NEXT", cmd_next},
	{"INDEX", cmd_index},
	{"SET", cmd_set},
};

void handlecmd(std::string_view s, mpvhandler &mpvh)
{
	auto name_start = std::find_if(s.begin(), s.end(), std::not_fn(isspace));
	auto args_start = std::find_if(name_start, s.end(), isspace);
	auto name = std::string_view(name_start, args_start - name_start);

	auto cmd = cmd_tab.find(name);
	if (cmd != cmd_tab.end())
		cmd->second(args_start, mpvh);
}

void cmd_pp(std::string_view args, mpvhandler &mpvh)
{
	UNUSED(args);
	mpvinfo i = mpvh_getinfo(&mpvh);
	i.state.paused = !i.state.paused;
	mpvh_set_state(&mpvh, i.state);
	i = mpvh_getinfo(&mpvh);
	mpvh_sendstatus(&mpvh);
}

void cmd_play(std::string_view args, mpvhandler &mpvh)
{
	UNUSED(args);
	mpvinfo i = mpvh_getinfo(&mpvh);
	i.state.paused = false;
	mpvh_set_state(&mpvh, i.state);
	i = mpvh_getinfo(&mpvh);
	mpvh_sendstatus(&mpvh);
}

void cmd_pause(std::string_view args, mpvhandler &mpvh)
{
	UNUSED(args);
	mpvinfo i = mpvh_getinfo(&mpvh);
	i.state.paused = true;
	mpvh_set_state(&mpvh, i.state);
	i = mpvh_getinfo(&mpvh);
	mpvh_sendstatus(&mpvh);
}

void cmd_status(std::string_view args, mpvhandler &mpvh)
{
	UNUSED(args);
	mpvh_sendstatus(&mpvh);
}

void cmd_seek(std::string_view args, mpvhandler &mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(&mpvh);
	i.state.time = time;
	mpvh_set_state(&mpvh, i.state);
	mpvh_sendstatus(&mpvh);
}

void cmd_seekplus(std::string_view args, mpvhandler &mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(&mpvh);
	i.state.time += time;
	mpvh_set_state(&mpvh, i.state);
	mpvh_sendstatus(&mpvh);
}

void cmd_seekminus(std::string_view args, mpvhandler &mpvh)
{
	double time = parsetime(args);
	mpvinfo i = mpvh_getinfo(&mpvh);
	i.state.time -= time;
	mpvh_set_state(&mpvh, i.state);
	mpvh_sendstatus(&mpvh);
}

void cmd_prev(std::string_view args, mpvhandler &mpvh)
{
	UNUSED(args);
	mpvinfo i = mpvh_getinfo(&mpvh);
	if (i.state.track - 1 < 0)
		return;
	playstate s = {};
	s.track = i.state.track - 1;
	s.paused = true;
	mpvh_set_state(&mpvh, s);
	mpvh_sendstatus(&mpvh);
}

void cmd_next(std::string_view args, mpvhandler &mpvh)
{
	UNUSED(args);
	mpvinfo i = mpvh_getinfo(&mpvh);
	if (i.state.track + 1 >= i.track_cnt)
		return;
	playstate s = {};
	s.track = i.state.track + 1;
	s.paused = true;
	mpvh_set_state(&mpvh, s);
	mpvh_sendstatus(&mpvh);
}

void cmd_index(std::string_view args, mpvhandler &mpvh)
{
	mpvinfo i = mpvh_getinfo(&mpvh);
	int track = std::stoi(args.begin()) - 1;
	if (!(0 <= track && track < i.track_cnt))
		return;
	playstate s = {};
	s.track = track;
	s.paused = true;
	mpvh_set_state(&mpvh, s);
	mpvh_sendstatus(&mpvh);
}
/*
void cmd_set(std::string_view args, mpvhandler &mpvh)
{
	std::regex r("(\\d+)\\D+(\\d+\\D+\\d+\\D+\\d+)\\D+(\\d+)");
	std::smatch match;
	std::string arg_str(args);
	if (!std::regex_search(arg_str, match, r))
		return;
	playstate s;
	s.track = std::stod(match[1]) - 1;
	s.time = parsetime(match[2].str());
	s.paused = std::stod(match[3]);
	mpvh_set_state(&mpvh, s);
	mpvh_sendstatus(&mpvh);
}
*/

void cmd_set(std::string_view args, mpvhandler &mpvh)
{
	regex_t r;
	regmatch_t match[4];
	regcomp(&r, "(\\d+)\\D+(\\d+\\D+\\d+\\D+\\d+)\\D+(\\d+)", REG_EXTENDED);
	if (!regexec(&r, args.data(), 4, match, 0))
		return;
	fprintf(stderr, "%d\n", match[1].rm_so);
	fprintf(stderr, "%d\n", match[2].rm_so);
	fprintf(stderr, "%d\n", match[3].rm_so);
	playstate s;
	s.track = std::stod(args.data() + match[1].rm_so) - 1;
	s.time = parsetime(args.data() + match[2].rm_so);
	s.paused = std::stod(args.data() + match[3].rm_so);
	mpvh_set_state(&mpvh, s);
	mpvh_sendstatus(&mpvh);
}