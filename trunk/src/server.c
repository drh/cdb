#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#undef __STRING
#include "fmt.h"
#include "symtab.h"
#include "mem.h"
#include "comm.h"

static char rcsid[] = "$Id$";

static SOCKET in, out;
static Nub_callback_T breakhandler;

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

static int equal(Nub_coord_T *src, sym_coordinate_ty w)  {
	/*
	Two coordinates are "equal" if their
	nondefault components are equal.
	*/
	return (src->y == 0 || src->y == w->y)
	    && (src->x == 0 || src->x == w->x)
	    && (src->file[0] == 0 || w->file && strcmp(src->file, w->file) == 0);
}

static void *coord2bpaddr(Nub_coord_T *src) {
	int i, count = Seq_length(pickles);

	for (i = 0; i < count; i++) {
		sym_module_ty pickle = Seq_get(pickles, i);
		int j, n = Seq_length(pickle->spoints);
		for (j = 0; j < n; j++) {
			sym_spoint_ty s = Seq_get(pickle->spoints, j);
			if (equal(src, s->src)) {
				count = Seq_length(modules);
				for (i = 0; i < count; i++) {
					struct module *m = Seq_get(modules, i);
					if (pickle->uname == m->uname)
						return (char *)NULL + j;
				}
				assert(0);
				return NULL;
			}
		}
	}
	assert(0);
	return NULL;
}

static void set_state(Nub_state_T *state) {
	struct sframe frame;
	sym_symbol_ty f;
	sym_module_ty m;
	sym_spoint_ty s;
	int n = _Nub_fetch(0, state->fp, &frame, sizeof frame);

	assert(n == sizeof frame);
	f = _Sym_symbol(frame.module, frame.func);
	assert(f);
	strncpy(state->name, f->id, sizeof state->name);
	m = _Sym_module(frame.module);
	assert(m);
	s = Seq_get(m->spoints, frame.ip);
	assert(s);
	strncpy(state->src.file, s->src->file, sizeof state->src.file);
	state->src.x = s->src->x; state->src.y = s->src->y;
	state->context = _Sym_symbol(frame.module, s->tail);
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
			set_state(&state);
			(*breakhandler)(state);
			break;
		case NUB_FAULT:
			recvmesg(in, &state, sizeof state);
			set_state(&state);
			fault(state);
			break;
		case NUB_QUIT: return;
		default: assert(0);
		}
	}
}

Nub_callback_T _Nub_set(Nub_coord_T src, Nub_callback_T onbreak) {
	Nub_callback_T prev = breakhandler;
	char flag = 1;
	int n = _Nub_store(1, coord2bpaddr(&src), &flag, 1);

	assert(n == 1);
	breakhandler = onbreak;
	return prev;
}

Nub_callback_T _Nub_remove(Nub_coord_T src) {
	char flag = 0;
	int n = _Nub_store(1, coord2bpaddr(&src), &flag, 1);

	assert(n == 1);
	return breakhandler;
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

int _Nub_frame(int n, Nub_state_T *state) {
	struct nub_frame args;

	args.n = n;
	sendout(NUB_FRAME, &args, sizeof args);
	recvmesg(in, &args, sizeof args);
	if (args.n >= 0)
		memcpy(state, &args.state, sizeof args.state);
	set_state(state);
	return args.n;
}

void _Nub_src(Nub_coord_T src,
	void apply(int i, const Nub_coord_T *src, void *cl), void *cl) {
	int i, k = 0, count = Seq_length(pickles);

	for (i = 0; i < count; i++) {
		sym_module_ty pickle = Seq_get(pickles, i);
		int j, n = Seq_length(pickle->spoints);
		for (j = 0; j < n; j++) {
			sym_spoint_ty s = Seq_get(pickle->spoints, j);
			if (equal(&src, s->src)) {
				Nub_coord_T z;
				strncpy(z.file, s->src->file, sizeof z.file);
				z.x = s->src->x; z.y = s->src->y;
				apply(k++, &z, cl);
			}
		}
	}
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
