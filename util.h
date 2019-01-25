#define UNUSED(x) (void)(x)

struct timestr { char str[40]; };

double parsetime(char *str, size_t len);
timestr sec_to_timestr(int seconds);