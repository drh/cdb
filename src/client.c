#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#undef __STRING
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

static void swtch(void);

static void onbreak(Nub_state_T state) {
	Header_T msg = NUB_BREAK;

	assert(out);
	tracemesg("%s: sending %s\n", identity, mesgname(msg));
	sendmesg(out, &msg, sizeof msg);
	sendmesg(out, &state, sizeof state);
	swtch();
}

static void swtch(void) {
	for (;;) {
		Header_T msg;
		recvmesg(in, &msg, sizeof msg);
		tracemesg("%s: switching on %s\n", identity, mesgname(msg));
		switch (msg) {
		case NUB_CONTINUE: return;
		case NUB_QUIT: out = 0; exit(EXIT_FAILURE); break;
		case NUB_FETCH: {
			int nbytes;
			struct nub_fetch args;
			recvmesg(in, &args, sizeof args);
			tracemesg("%s: _Nub_fetch(space=%d,address=%p,buf=NULL,nbytes=%d)\n",
				 identity, args.space, args.address, args.nbytes);
			nbytes = _Nub_fetch(args.space, args.address, NULL, args.nbytes);
			sendmesg(out, &nbytes, sizeof nbytes);
			sendmesg(out, args.address, nbytes);
			break;
		}
		case NUB_STORE: {
			struct nub_store args;
			recvmesg(in, &args, sizeof args);
			tracemesg("%s: _Nub_store(space=%d,address=%p,buf=%p,nbytes=%d)\n",
				 identity, args.space, args.address, args.buf, args.nbytes);
			args.nbytes = _Nub_store(args.space, args.address, args.buf, args.nbytes);
			sendmesg(out, &args.nbytes, sizeof args.nbytes);
			break;
		}
		case NUB_FRAME: {
			struct nub_frame args;
			recvmesg(in, &args, sizeof args);
			args.n = _Nub_frame(args.n, &args.state);
			sendmesg(out, &args, sizeof args);
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
		tracemesg("%s: sending %s\n", identity, mesgname(msg));
		sendmesg(out, &msg, sizeof msg);
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
	tracemesg("%s: sending %s\n", identity, mesgname(msg));
	sendmesg(out, &msg, sizeof msg);
	sendmesg(out, &state, sizeof state);
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
		extern Nub_callback_T breakhandler;
		breakhandler = onbreak;
		tracemesg("%s: sending %s\n", identity, mesgname(msg));
		sendmesg(out, &msg, sizeof msg);
		sendmesg(out, &state, sizeof state);
		swtch();
	}
}
