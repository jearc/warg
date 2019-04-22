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
	long tok[3];
	size_t tokidx = 0;

	char *tokstart = str;
	while (1) {
		while (*tokstart && !isdigit(*tokstart))
			tokstart++;
		if (!*tokstart)
			break;
		
		char *tokend = tokstart;
		while (isdigit(*tokend + 1))
			tokend++;

		tok[tokidx++] = strtol(tokstart, &tokend, 10);

		if (tokidx > 2)
			break;
		
		tokstart = tokend + 1;
	}

	double seconds = 0;
	for (size_t i = 0; i < tokidx; i++)
		seconds += tok[i] * pow(60, tokidx - i - 1);

	return seconds;
}

timestr sec_to_timestr(int seconds)
{
	timestr ts;
	
	int hh = seconds / 3600;
	int mm = (seconds % 3600) / 60;
	int ss = seconds % 60;
	
	if (hh)
		snprintf(ts.str, 39, "%d:%02d:%02d", hh, mm, ss);
	else
		snprintf(ts.str, 39, "%d:%02d", mm, ss);
		
	return ts;
}