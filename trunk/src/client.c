#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "comm.h"

#ifdef unix
static int WSAGetLastError(void) {
	return errno;
}
#endif

static char rcsid[] = "$Id$";

static SOCKET in, out;

static char *stringf(const char *fmt, ...) {
	static char buf[1024];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsprintf(buf, fmt, ap);
	va_end(ap);
	return buf;
}

static void sendeach(int i, Nub_coord_T *src, void *cl) {
	sendmsg(out, src, *(int *)cl);
}

static void swtch(void);

static void onbreak(Nub_state_T state) {
	Header_T msg = NUB_BREAK;

	assert(out);
	tracemsg("%s: sending %s\n", identity, msgname(msg));
	sendmsg(out, &msg, sizeof msg);
	sendmsg(out, &state, sizeof state);
	swtch();
}

static void swtch(void) {
	for (;;) {
		Header_T msg;
		recvmsg(in, &msg, sizeof msg);
		tracemsg("%s: switching on %s\n", identity, msgname(msg));
		switch (msg) {
		case NUB_CONTINUE: return;
		case NUB_QUIT: out = 0; exit(EXIT_FAILURE); break;
		case NUB_SET: {
			Nub_coord_T args;
			recvmsg(in, &args, sizeof args);
			_Nub_set(args, onbreak);
			break;
		}
		case NUB_REMOVE: {
			Nub_coord_T args;
			recvmsg(in, &args, sizeof args);
			_Nub_remove(args);
			break;
		}
		case NUB_FETCH: {
			int nbytes;
			char buf[1024];
			struct nub_fetch args;
			recvmsg(in, &args, sizeof args);
			if (args.nbytes > sizeof buf)
				args.nbytes = sizeof buf;
			nbytes = _Nub_fetch(args.space, args.address, buf, args.nbytes);
			sendmsg(out, &nbytes, sizeof nbytes);
			sendmsg(out, buf, nbytes);
			break;
		}
		case NUB_STORE: {
			struct nub_store args;
			recvmsg(in, &args, sizeof args);
			args.nbytes = _Nub_store(args.space, args.address, args.buf, args.nbytes);
			sendmsg(out, &args.nbytes, sizeof args.nbytes);
			break;
		}
		case NUB_FRAME: {
			struct nub_frame args;
			recvmsg(in, &args, sizeof args);
			args.n = _Nub_frame(args.n, &args.state);
			sendmsg(out, &args, sizeof args);
			break;
		}
		case NUB_SRC: {
			Nub_coord_T src;
			int size = sizeof src;
			recvmsg(in, &src, sizeof src);
			_Nub_src(src, sendeach, &size);
			src.y = 0;
			sendmsg(out, &src, sizeof src);
			break;
		}
		default: assert(0);
		}
	}
}

static int connectto(const char *host, short port) {
	struct sockaddr_in server, client;
	struct hostent *hp;
	int len;
	unsigned long inaddr;

	in = socket(AF_INET, SOCK_STREAM, 0);
	if (in == SOCKET_ERROR) {
		perror(stringf("%s: socket (%d)", identity, WSAGetLastError()));
		return EXIT_FAILURE;
	}
	memset(&server, 0, sizeof server);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	inaddr = inet_addr(host);
	if (inaddr != INADDR_NONE)
		memcpy(&server.sin_addr, &inaddr, sizeof inaddr);
	else if ((hp = gethostbyname(host)) != NULL)
		memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
	else {
		fprintf(stderr, "%s: gethostbyname (%d)\n", identity, WSAGetLastError());
		return EXIT_FAILURE;
	}
	if (connect(in, (struct sockaddr *)&server, sizeof server) != 0) {
		perror(stringf("%s: connect (%d)", identity, WSAGetLastError()));
		return EXIT_FAILURE;
	}
	len = sizeof client;
	if (getsockname(in, (struct sockaddr *)&client, &len) < 0) {
		perror(stringf("%s: getsockname (%d)", identity, WSAGetLastError()));
		return EXIT_FAILURE;
	}
	fprintf(stderr, "%s: connected on %s:%d ", identity,
		inet_ntoa(client.sin_addr), ntohs(client.sin_port));
	fprintf(stderr, "to %s:%d\n",
		inet_ntoa(server.sin_addr), ntohs(server.sin_port));
	out = in;
	return EXIT_SUCCESS;
}

static void cleanup(void) {
	if (out) {
		Header_T msg = NUB_QUIT;
		tracemsg("%s: sending %s\n", identity, msgname(msg));
		sendmsg(out, &msg, sizeof msg);
		closesocket(in);
	}
#ifdef _WIN32
	WSACleanup();
#endif
}

void _Cdb_fault(Nub_state_T state) {
	Header_T msg = NUB_FAULT;

	if (out == 0)
		exit(EXIT_FAILURE);
	tracemsg("%s: sending %s\n", identity, msgname(msg));
	sendmsg(out, &msg, sizeof msg);
	sendmsg(out, &state, sizeof state);
	swtch();
}

void _Cdb_startup(Nub_state_T state) {
	char *host, *s;
	short port = 9001;

	if ((host = getenv("DEBUGGER")) == NULL)
		return;
	s = strchr(host, ':');
	if (s != NULL && s[1] != '\0') {
		*s = '\0';
		port = atoi(s + 1);
	}
#if _WIN32
	{
		WSADATA wsaData;
		int err = WSAStartup(MAKEWORD(2, 0), &wsaData);
		if (err != 0) {
			fprintf(stderr, "%s: WSAStartup (%d)", identity, err);
			return;
		}
	}
#endif
	atexit(cleanup);
	if ((s = getenv("TRACE")) != NULL)
		trace = atoi(s);
	identity = "client";
	if (connectto(host, port) == EXIT_SUCCESS) {	/* start the nub */
		Header_T msg = NUB_STARTUP;
		tracemsg("%s: sending %s\n", identity, msgname(msg));
		sendmsg(out, &msg, sizeof msg);
		sendmsg(out, &state, sizeof state);
		swtch();
	}
}
