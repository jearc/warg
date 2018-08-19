struct chatlog;

void sendmsg(const char *text);
void logmsg(chatlog *cl, char *username, size_t username_len, char *text, size_t text_len);
int parsemsg(chatlog *cl, mpv_handle *mpv, char *buf, size_t len);
void handlemsg(chatlog *cl, mpv_handle *mpv, char *username, 
	size_t username_len, char *text, size_t text_len);