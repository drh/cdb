#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "server.h"
#include "mem.h"
#include "seq.h"

static char rcsid[] = "$Id$";

extern int read(int, char *, int);
extern int write(int, char *, int);
extern int pipe(int []);
extern int close(int);
extern int fork(void);
extern int execl(char *, char *, ...);

static int in, out, trace;

static Nub_callback_T breakhandler;

static char *msgname(Header_T msg) {
	static char buf[120], *names[] = {
#define xx(name) #name,
	messagecodes
#undef xx
	};
	if (msg >= 0 && msg < sizeof names/sizeof names[0])
		return names[msg];
	else
		sprintf(buf, "unknown message %d", msg);
	return buf;
}

static void tracemsg(char *fmt, ...) {
	if (trace) {
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		trace--;
	}
}

static void recv(void *buf, int size) {
	int n;

	n = read(in, buf, size);
	tracemsg("client: received %d bytes\n", n);
	assert(n == size);
}

static void send(int op, void *buf, int size) {
	int n;
	Header_T msg;

	msg = op;
	tracemsg("client: sending %s\n", msgname(msg));
	n = write(out, (void*)&msg, sizeof msg);
	assert(n == sizeof msg);
	if (size > 0) {
		assert(buf);
		tracemsg("client: sending %d bytes\n", size);
		n = write(out, buf, size);
		assert(n == size);
	}
}

static void cleanup(void) {
	if (out)
		send(NUB_QUIT, NULL, 0);
}

void _Nub_init(Nub_callback_T startup, Nub_callback_T fault) {
	Header_T msg;
	Nub_state_T state;
	char *s;

	atexit(cleanup);
	if ((s = getenv("TRACE")) != NULL)
		trace = atoi(s);
	recv(&msg, sizeof msg);
	assert(msg == NUB_STARTUP);
	recv(&state, sizeof state);
	startup(state);
	for (;;) {
		send(NUB_CONTINUE, NULL, 0);
		recv(&msg, sizeof msg);
		switch (msg) {
		case NUB_BREAK:
			recv(&state, sizeof state);
			(*breakhandler)(state);
			break;
		case NUB_FAULT:
			recv(&state, sizeof state);
			fault(state);
			break;
		case NUB_QUIT: out = 0; return;
		default: assert(0);
		}
	}
}

Nub_callback_T _Nub_set(Nub_coord_T src, Nub_callback_T onbreak) {
	Nub_callback_T prev = breakhandler;

	breakhandler = onbreak;
	send(NUB_SET, &src, sizeof src);
	return prev;
}

Nub_callback_T _Nub_remove(Nub_coord_T src) {
	send(NUB_REMOVE, &src, sizeof src);
	return breakhandler;
}

int _Nub_fetch(int space, void *address, void *buf, int nbytes) {
	struct nub_fetch args;

	args.space = space;
	args.address = address;
	args.nbytes = nbytes;
	send(NUB_FETCH, &args, sizeof args);
	recv(&args.nbytes, sizeof args.nbytes);
	recv(buf, args.nbytes);
	return args.nbytes;
}

int _Nub_store(int space, void *address, void *buf, int nbytes) {
	struct nub_store args;

	args.space = space;
	args.address = address;
	args.nbytes = nbytes;
	assert(nbytes <= sizeof args.buf);
	memcpy(args.buf, buf, nbytes);
	send(NUB_STORE, &args, sizeof args);
	recv(&args.nbytes, sizeof args.nbytes);
	return args.nbytes;
}

int _Nub_frame(int n, Nub_state_T *state) {
	struct nub_frame args;

	args.n = n;
	send(NUB_FRAME, &args, sizeof args);
	recv(&args, sizeof args);
	if (args.n >= 0)
		memcpy(state, &args.state, sizeof args.state);
	return args.n;
}

void _Nub_src(Nub_coord_T src,
	void apply(int i, Nub_coord_T *src, void *cl), void *cl) {
	int i = 0, n;
	static Seq_T srcs;

	if (srcs == NULL)
		srcs = Seq_new(0);
	n = Seq_length(srcs);
	send(NUB_SRC, &src, sizeof src);
	recv(&src, sizeof src);
	while (src.y) {
		Nub_coord_T *p;
		if (i < n)
			p = Seq_get(srcs, i);
		else {
			Seq_addhi(srcs, NEW(p));
			n++;
		}
		*p = src;
		i++;
		recv(&src, sizeof src);
	}
	for (n = 0; n < i; n++)
		apply(n, Seq_get(srcs, n), cl);
}

extern void _Cdb_startup(Nub_state_T state), _Cdb_fault(Nub_state_T state);

int main(int argc, char *argv[], char *envp[]) {
	int i;

	for (i = 0; i < argc; i++) {
		printf("%s ", argv[i]);
		if (strncmp(argv[i], "-in=", 4) == 0)
			in = atoi(argv[i]+4);
		else if (strncmp(argv[i], "-out=", 5) == 0)
			out = atoi(argv[i]+5);
	}
	printf("\n");
	assert(in);
	assert(out);
	_Nub_init(_Cdb_startup, _Cdb_fault);
	return EXIT_SUCCESS;
}
