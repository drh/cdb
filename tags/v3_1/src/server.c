#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "fmt.h"
#include "comm.h"

static char rcsid[] = "$Id$";

static SOCKET in, out;
Nub_callback_T breakhandler;

static void sendout(int op, const void *buf, int size) {
	Header_T msg;

	msg = op;
	tracemesg("%s: sending %s\n", identity, mesgname(msg));
	sendmesg(out, &msg, sizeof msg);
	if (size > 0) {
		assert(buf);
		sendmesg(out, buf, size);
	}
}

int _Nub_frame(int n, Nub_state_T *state) {
	struct nub_frame args;

	args.n = n;
	sendout(NUB_FRAME, &args, sizeof args);
	recvmesg(in, &args, sizeof args);
	if (args.n >= 0)
		memcpy(state, &args.state, sizeof args.state);
	_Nub_state(state, NULL);
	return args.n;
}

void _Nub_init(Nub_callback_T startup, Nub_callback_T fault) {
	Header_T msg;
	Nub_state_T state;

	recvmesg(in, &msg, sizeof msg);
	assert(msg == NUB_STARTUP);
	recvmesg(in, &state, sizeof state);
	startup(state);
	for (;;) {
		sendout(NUB_CONTINUE, NULL, 0);
		recvmesg(in, &msg, sizeof msg);
		tracemesg("%s: switching on %s\n", identity, mesgname(msg));
		switch (msg) {
		case NUB_BREAK:
			recvmesg(in, &state, sizeof state);
			(*breakhandler)(state);
			break;
		case NUB_FAULT:
			recvmesg(in, &state, sizeof state);
			_Nub_state(&state, NULL);
			fault(state);
			break;
		case NUB_QUIT: return;
		default: assert(0);
		}
	}
}

int _Nub_fetch(int space, const void *address, void *buf, int nbytes) {
	struct nub_fetch args;

	args.space = space;
	args.address = address;
	args.nbytes = nbytes;
	tracemesg("%s: _Nub_fetch(space=%d,address=%p,buf=%p,nbytes=%d)\n",
		 identity, space, address, buf, nbytes);
	sendout(NUB_FETCH, &args, sizeof args);
	recvmesg(in, &args.nbytes, sizeof args.nbytes);
	recvmesg(in, buf, args.nbytes);
	return args.nbytes;
}

int _Nub_store(int space, void *address, const void *buf, int nbytes) {
	struct nub_store args;

	args.space = space;
	args.address = address;
	args.nbytes = nbytes;
	assert(nbytes <= sizeof args.buf);
	memcpy(args.buf, buf, nbytes);
	tracemesg("%s: _Nub_store(space=%d,address=%p,buf=%p,nbytes=%d)\n",
		 identity, space, address, buf, nbytes);
	sendout(NUB_STORE, &args, sizeof args);
	recvmesg(in, &args.nbytes, sizeof args.nbytes);
	return args.nbytes;
}

extern void _Cdb_startup(Nub_state_T state), _Cdb_fault(Nub_state_T state);

static void cleanup(void) {
	if (out) {
		sendout(NUB_QUIT, NULL, 0);
		closesocket(out);
	}
#ifdef _WIN32
	WSACleanup();
#endif
}

#ifdef unix
static int WSAGetLastError(void) {
	return errno;
}
#endif

static int server(short port) {
	struct sockaddr_in server;
	SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd == SOCKET_ERROR) {
		perror(Fmt_string("%s: socket (%d)", identity, WSAGetLastError()));
		return EXIT_FAILURE;
	}
	memset(&server, 0, sizeof server);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port);
	if (bind(fd, (struct sockaddr *)&server, sizeof server) != 0) {
		perror(Fmt_string("%s: bind (%d)", identity, WSAGetLastError()));
		return EXIT_FAILURE;
	}
	printf("%s listening on %s:%d\n", identity, inet_ntoa(server.sin_addr), ntohs(server.sin_port));
	listen(fd, 5);
	for (;;) {
		struct sockaddr_in client;
		int len = sizeof client;
		in = accept(fd, (struct sockaddr *)&client, &len);
		if (in == SOCKET_ERROR) {
			perror(Fmt_string("%s: accept (%d)", identity, WSAGetLastError()));
			return EXIT_FAILURE;
		}
		printf("%s: now serving %s:%d\n", identity, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
		out = in;
		_Nub_init(_Cdb_startup, _Cdb_fault);
		out = 0;
		closesocket(in);
		printf("%s: disconnected\n", identity);
	}
}

int main(int argc, char *argv[]) {
	short port = 9001;
	int i;
	char *s;

	if ((s = getenv("TRACE")) != NULL)
		trace = atoi(s);
	identity = argv[0];
#if _WIN32
	{
		WSADATA wsaData;
		int err = WSAStartup(MAKEWORD(2, 0), &wsaData);
		if (err != 0) {
			fprintf(stderr, "%s: WSAStartup (%d)", identity, err);
			return EXIT_FAILURE;
		}
	}
#endif
	atexit(cleanup);
	for (i = 1; i < argc; i++)
		if (isdigit(*argv[i]))
			port = atoi(argv[i]);
		else if (strncmp(argv[i], "-trace=", 7) == 0)
			trace = atoi(argv[i] + 7);
	return server(port);
}
