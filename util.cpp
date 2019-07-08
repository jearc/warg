#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "moov.h"

void die(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

double parsetime(char *str)
{
	double sec = 0;
	for (int i = 0; i < 3; i++) {
		while (*str && !isdigit(*str))
			str++;
		if (!*str)
			break;

		sec *= 60;
		sec += strtol(str, &str, 10);
		str++;
	}
	return sec;
}

timestr sec_to_timestr(unsigned int sec)
{
	unsigned int h, m, s;
	h = sec / 3600;
	m = (sec % 3600) / 60;
	s = sec % 60;

	timestr ts;
	snprintf(ts.str, TIMESTR_BUF_LEN, "%u:%02u:%02u", h, m, s);
	return ts;
}
