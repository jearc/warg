#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <iostream>
#include <functional>
#include <fmt/format.h>

#include "moov.h"

void die(const std::string_view error_message)
{
	fmt::print("{}\n", error_message);
	exit(EXIT_FAILURE);
}

double parsetime(const std::string_view str)
{
	double sec = 0;
	auto it = str.begin();
	
	for (int i = 0; i < 3; i++) {
		it = std::find_if(it, str.end(), isdigit);
		if (it == str.end())
			break;
		sec *= 60;
		size_t len;
		sec += std::stod(it, &len);
		it += len + 1;
	}
	return sec;
}

std::string sectoa(unsigned int sec)
{
	unsigned int h, m, s;
	h = sec / 3600;
	m = (sec % 3600) / 60;
	s = sec % 60;
	
	if (h)
		return fmt::format("{}:{:02}:{:02}", h, m, s);
	return fmt::format("{:02}:{:02}", m, s);
}

void sendmsg(const std::string_view msg)
{
	fmt::print("{}", msg);
	fputc('\0', stdout);
	fflush(stdout);
}
