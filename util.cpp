#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>
#include <string>

void die(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

std::string sec_to_timestr(unsigned int sec)
{
	unsigned int h, m, s;
	h = sec / 3600;
	m = (sec % 3600) / 60;
	s = sec % 60;

	char buf[15] = { 0 };
	snprintf(buf, 15, "%02u:%02u:%02u", h, m, s);
	return std::string(buf);
}
