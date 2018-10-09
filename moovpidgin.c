#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <systemd/sd-bus.h>
#include <pthread.h>
#include <ctype.h>
#include "util.h"

const char *purple_service = "im.pidgin.purple.PurpleService";
const char *purple_object = "/im/pidgin/purple/PurpleObject";
const char *received_match_rule =
	"sender='im.pidgin.purple.PurpleService',member='ReceivedImMsg'";
const char *sent_match_rule =
	"sender='im.pidgin.purple.PurpleService',member='SentImMsg'";
const char *video_filetypes[] = { "mp4", "mkv", "avi", "ogm" };

struct state {
	pthread_mutex_t mtx;
	sd_bus *bus;
	bool pending_open;
	int acc;
	int conv;
	pid_t moovpid;
	int moovin[2];
	int moovout[2];
	pthread_t moovthread;
};

void launchmoov(struct state *state, size_t argc, char **argv);
int send_purple(sd_bus *bus, int conversation, const char *msg);
int sendmoov(int f, char *alias, char *msg);
void handlecmd(struct state *state, int acc, int conv, char *msg, bool self);
void *handlemoov(void *data);
void cmd_moov(struct state *state, int acc, int conv, char *args, bool self);
void cmd_yt(struct state *state, int acc, int conv, char *args, bool self);
void cmd_resume(struct state *state, int acc, int conv, char *args, bool self);
void cmd_sure(struct state *state, int acc, int conv, char *args, bool self);

struct {
	const char *name;
	void (*fn)(struct state *, int, int, char *, bool);
} cmdtab[] = {
	{ "MOOV", cmd_moov },
	{ "YT", cmd_yt },
	{ "RESUME", cmd_resume },
	{ "sure", cmd_sure }
};

void cmd_moov(struct state *state, int acc, int conv, char *args, bool self)
{
	UNUSED(state);
	UNUSED(acc);
	UNUSED(conv);
	UNUSED(args);
	UNUSED(self);
}

char *advance_to(char *str, char chr)
{
	if (!str)
		return NULL;
	
	while (*str != '\0' && *str != chr)
		str++;
	
	return str;
}

void striphtml(char *str)
{
	char *tagopen;
	while (*(tagopen = advance_to(str, '<'))) {
		char *tagclose = advance_to(tagopen, '>') + 1;
		strcpy(tagopen, tagclose);
		str = tagopen;
	}
}

char *firstnonspace(char *str)
{
	if (!str)
		return NULL;

	while (*str != '\0' && isspace(*str))
		str++;

	return str;
}

char *lastnonspace(char *str)
{
	if (!str)
		return NULL;

	while (*str != '\0' && !isspace(*str))
		str++;

	return str;
}

void cmd_yt(struct state *state, int acc, int conv, char *args, bool self)
{
	UNUSED(self);

	char *urlstart;
	char *urlend;
	char *timestart;
	char *timeend;

	urlstart = firstnonspace(args);
	urlend = lastnonspace(urlstart);
	if (urlstart == urlend)
		return;

	timestart = firstnonspace(urlend);
	timeend = lastnonspace(timestart);

	*urlend = '\0';
	*timeend = '\0';

	size_t argc = timestart < timeend ? 3 : 1;
	char *argv[3];
	argv[0] = urlstart;
	argv[1] = "-s";
	argv[2] = timestart;

	launchmoov(state, argc, argv);
	state->conv = conv;
	state->acc = acc;
}

void cmd_resume(struct state *state, int acc, int conv, char *args, bool self)
{
	UNUSED(state);
	UNUSED(acc);
	UNUSED(conv);
	UNUSED(args);
	UNUSED(self);
}

void cmd_sure(struct state *state, int acc, int conv, char *args, bool self)
{
	UNUSED(state);
	UNUSED(acc);
	UNUSED(conv);
	UNUSED(args);
	UNUSED(self);
}

void launchmoov(struct state *state, size_t argc, char **argv)
{
	pid_t moovpid;

	pipe(state->moovin);
	pipe(state->moovout);

	moovpid = fork();

	if (moovpid > 0) {
		state->moovpid = moovpid;
		
		close(state->moovin[0]);
		close(state->moovout[1]);

		pthread_create(&state->moovthread, NULL, handlemoov, &state);

		return;
	}
	
	close(0);
	close(1);
	close(state->moovin[1]);
	close(state->moovout[0]);
	dup(state->moovin[0]);
	dup(state->moovout[1]);

	/* char *argv_[100] = {0};
	argv_[0] = "moov";
	size_t i;
	for (i = 1; i < argc && i < 99; i++)
		argv_[i] = argv[i - 1];
	argv_[i] = NULL;
	*argv_[i] = NULL;

	char **argv__ = argv_;
	while (argv__++)
		fprintf(stderr, "%s\n", *argv__);
	*/
	
	execv("./moov", "https://www.youtube.com/watch?v=AbOdvJkMt0E");
}

char *getalias(sd_bus *bus, int account)
{
	int r;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;

	r = sd_bus_call_method(bus, purple_service, purple_object, NULL,
		"PurpleAccountGetAlias", &err, &m, "i", account);
	if (r < 0)
		fprintf(strerror, "failed to call PurpleAccountGetAlias: %s.\n",
			strerror(-r));

	char *alias;
	r = sd_bus_message_read(m, "s", &alias);
	if (r < 0)
		fprintf(strerror, "failed to read PurpleAccountGetAlias response: %s.\n",
			strerror(-r));

	//sd_bus_message_unref(m);
	//sd_bus_error_free(&err);

	return alias;
}

int getconvim(sd_bus *bus, int conversation)
{
	int r;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;

	int convim;
	r = sd_bus_call_method(bus, purple_service, purple_object, NULL, "PurpleConvIm",
		&err, &m, "i", conversation);
	if (r < 0)
		fprintf(stderr, "failed to call PurpleConvIm method: %s.\n",
			strerror(-r));
	r = sd_bus_message_read(m, "i", &convim);
	if (r < 0)
		fprintf(stderr, "failed to read message from PurpleConvIm: %s.\n",
			strerror(-r));

	//sd_bus_message_unref(m);
	//sd_bus_error_free(&err);

	return convim;
}

char *getconvtitle(sd_bus *bus, int conversation)
{
	int r;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;

	r = sd_bus_call_method(bus, purple_service, purple_object, NULL,
		"PurpleConversationGetTitle", &err, &m, "i", conversation);
	if (r < 0)
		fprintf(strerror, "failed to call PurpleConversationGetTitle: %s.\n",
			strerror(-r));

	char *convtitle;
	r = sd_bus_message_read(m, "s", &convtitle);
	if (r < 0)
		fprintf(strerror, "failed to read response from PurpleConversationGetTitle"
			": %s.\n", strerror(-r));

	//sd_bus_message_unref(m);
	//sd_bus_error_free(&err);

	return convtitle;
}

int convofrecp(sd_bus *bus, char *recp)
{
	int r;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;

	r = sd_bus_call_method(bus, purple_service, purple_object, NULL,
		"PurpleGetConversations", &err, &m, NULL);
	if (r < 0)
		fprintf(stderr, "failed to call PurpleGetConversations: %s\n",
			strerror(-r));
	const int *conv;
	size_t nconv;
	r = sd_bus_message_read_array(m, 'i', (const void **)&conv, &nconv);
	if (r < 0)
		fprintf(stderr, "failed to read PurpleGetConversations response: %s\n",
			strerror(-r));

	for (size_t i = 0; i < nconv; i++) {
		sd_bus_message *m1 = NULL;
		r = sd_bus_call_method(bus, purple_service, purple_object, NULL,
			"PurpleConversationGetName", &err, &m1, "i", conv[i]);
		if (r < 0)
			fprintf(stderr, "failed to call PurpleConversationGetName: %s\n",
				strerror(-r));
		char *name;
		r = sd_bus_message_read(m1, "s", &name);
		if (r < 0)
			fprintf(stderr, "failed to read PurpleConversationGetName"
				"response: %s.\n", strerror(-r));
		if (strcmp(recp, name) == 0)
			return conv[i];
	}

	return -1;
}

int sent_cb(sd_bus_message *m, void *data, sd_bus_error *err)
{
	struct state *state = data;
	pthread_mutex_lock(&state->mtx);

	int r;
	int account;
	char *recipient, *message;
 	r = sd_bus_message_read(m, "iss", &account, &recipient, &message);
	if (r < 0)
		fprintf(stderr, "failed to read sent message: %s.\n", strerror(-r));

	fprintf(stderr, "prestrip: %s\n", message);
	striphtml(message);
	fprintf(stderr, "poststrip: %s\n", message);

	char *alias = getalias(state->bus, account);
	int conv = convofrecp(state->bus, recipient);
	if (conv == state->conv && kill(state->moovpid, 0))
        	sendmoov(state->moovin[1], alias, message);
	handlecmd(state, account, conv, message, true);

	pthread_mutex_unlock(&state->mtx);
}

int received_cb(sd_bus_message *m, void *data, sd_bus_error *err)
{
	struct state *state = data;
	pthread_mutex_lock(&state->mtx);

	int r;
	
	int acc, conv;
	char *sender, *msg;
	uint32_t flags;
	r = sd_bus_message_read(m, "issiu", &acc, &sender, &msg, &conv, &flags);
	if (r < 0)
		fprintf(stderr, "failed to read received message: %s.\n", strerror(-r));

	striphtml(msg);

	char *convtitle = getconvtitle(state->bus, conv);

	if (conv == state->conv && kill(state->moovpid, 0))
		sendmoov(state->moovin[1], convtitle, msg);

	pthread_mutex_unlock(&state->mtx);
}

int send_purple(sd_bus *bus, int im, const char *msg)
{
	int r;
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;

	r = sd_bus_call_method(bus, purple_service, purple_object, NULL, "PurpleConvImSend",
		&err, &m, "is", im, msg);
	if (r < 0)
		fprintf(stderr, "failed to send purple message: %s.", strerror(-r));

	return r;
}

int sendmoov(int f, char *alias, char *msg)
{
        write(f, alias, strlen(alias));
	write(f, ":", 1);
	write(f, msg, strlen(msg) + 1);
}

void handlecmd(struct state *state, int acc, int conv, char *msg, bool self)
{
	static size_t ncmd = sizeof cmdtab / sizeof cmdtab[0];

	while (isspace(*msg))
		msg++;

	size_t cmdlen = 0;
	while (msg[cmdlen] && !isspace(msg[cmdlen]))
		cmdlen++;

	char *args = msg + cmdlen;
	while (*args != '\0' && isspace(*args))
		args++;

	for (size_t i = 0; i < ncmd; i++) {
		if (strstr(msg, cmdtab[i].name) != msg)
			continue;
		if (cmdlen != strlen(cmdtab[i].name))
			continue;
		cmdtab[i].fn(state, acc, conv, args, self);
		break;
	}
}

void *handlemoov(void *data)
{
	struct state *state = data;

	char buf[1000] = {0};
	size_t bufidx = 0;
	for (;;) {
		char c;
		read(state->moovout[0], &c, 1);
		if (c == '\0') {
			//fprintf(stderr, "%s\n", buf);
			//send_purple(state->bus, getconvim(state->bus, state->conv), buf);
			memset(buf, 0, 1000);
			bufidx = 0;
		}
		else if (bufidx < 998)
			buf[bufidx++] = c;
	}
}

int main(int argc, char *argv[])
{
	int r;
	struct state state = {
		.mtx = PTHREAD_MUTEX_INITIALIZER,
		.bus = NULL,
		.acc = -1,
		.conv = -1
	};

	fprintf(stderr, "Starting moovpidgin.\n");

	r = sd_bus_default_user(&state.bus);
	if (r < 0) {
		fprintf(stderr, "Error: failed to connect to user bus: %s.\n",
			strerror(-r));
		goto finish;
	}

	r = sd_bus_add_match(state.bus, NULL, sent_match_rule, sent_cb, &state);
	if (r < 0) {
		fprintf(stderr, "Error: failed to set sent message callback: %s.\n",
			strerror(-1));
		goto finish;
	}

	r = sd_bus_add_match(state.bus, NULL, received_match_rule, received_cb, &state);
	if (r < 0) {
		fprintf(stderr, "Error: failed to set received message callback: %s.\n",
			strerror(-r));
		goto finish;
	}

	for (;;) {
		r = sd_bus_process(state.bus, NULL);
		if (r < 0) {
			fprintf(stderr, "Failed to process bus: %s.\n", strerror(-r));
			goto finish;
		}
		if (r > 0)
			continue;

		r = sd_bus_wait(state.bus, (uint64_t)-1);
		if (r < 0) {
			fprintf(stderr, "Failed to wait on bus: %s.\n", strerror(-r));
			goto finish;
		}
	}

finish:
	sd_bus_unref(state.bus);
}