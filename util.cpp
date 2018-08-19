#include <math.h>
#include <ctype.h>

double parsetime(char *str, size_t len)
{
	char *strend = str + len + 1;

	long tok[3];
	size_t tokidx = 0;

	char *tokstart = str;
	while (1) {
		while (tokstart != strend && !isdigit(*tokstart))
			tokstart++;
		if (tokstart == strend)
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