#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "server.h"

static char rcsid[] = "$Id$";

extern int read(int, char *, int);
extern int write(int, char *, int);
extern int pipe(int []);
extern int close(int);
extern int fork(void);
extern int execl(char *, char *, ...);

static int in, out, trace;

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
	tracemsg("server: received %d bytes\n", n);
	assert(n == size);
}

static void send(void *buf, int size) {
	int n;

	tracemsg("server: sending %d bytes\n", size);
	n = write(out, buf, size);
	assert(n == size);
}

static void sendeach(int i, Nub_coord_T *src, void *cl) {
	send(src, *(int *)cl);
}

static void swtch(void);

static void onbreak(Nub_state_T state) {
	Header_T msg = NUB_BREAK;

	assert(out);
	tracemsg("server: sending %s\n", msgname(msg));
	send(&msg, sizeof msg);
	send(&state, sizeof state);
	swtch();
}

static void swtch(void) {
	for (;;) {
		Header_T msg;
		recv(&msg, sizeof msg);
		tracemsg("server: switching on %s\n", msgname(msg));
		switch (msg) {
		case NUB_CONTINUE: return;
		case NUB_QUIT: out = 0; exit(EXIT_FAILURE); break;
		case NUB_SET: {
			Nub_coord_T args;
			recv(&args, sizeof args);
			_Nub_set(args, onbreak);
			break;
		}
		case NUB_REMOVE: {
			Nub_coord_T args;
			recv(&args, sizeof args);
			_Nub_remove(args);
			break;
		}
		case NUB_FETCH: {
			int nbytes;
			char buf[1024];
			struct nub_fetch args;
			recv(&args, sizeof args);
			if (args.nbytes > sizeof buf)
				args.nbytes = sizeof buf;
			nbytes = _Nub_fetch(args.space, args.address, buf, args.nbytes);
			send(&nbytes, sizeof nbytes);
			send(buf, nbytes);
			break;
		}
		case NUB_STORE: {
			struct nub_store args;
			recv(&args, sizeof args);
			args.nbytes = _Nub_store(args.space, args.address, args.buf, args.nbytes);
			send(&args.nbytes, sizeof args.nbytes);
			break;
		}
		case NUB_FRAME: {
			struct nub_frame args;
			recv(&args, sizeof args);
			args.n = _Nub_frame(args.n, &args.state);
			send(&args, sizeof args);
			break;
		}
		case NUB_SRC: {
			Nub_coord_T src;
			int size = sizeof src;
			recv(&src, sizeof src);
			_Nub_src(src, sendeach, &size);
			src.y = 0;
			send(&src, sizeof src);
			break;
		}
		default: assert(0);
		}
	}
}

static void cleanup(void) {
	if (out) {
		Header_T msg = NUB_QUIT;
		tracemsg("server: sending %s\n", msgname(msg));
		send(&msg, sizeof msg);
	}
}

void _Cdb_startup(Nub_state_T state) {
	char *cdb;
	int fd1[2], fd2[2];

	if ((cdb = getenv("DEBUGGER")) == NULL)
		return;
	if (pipe(fd1) < 0)
		return;
	if (pipe(fd2) < 0) {
		close(fd1[0]); close(fd1[1]);
		return;
	}
	switch (fork()) {
	case -1:
		close(fd1[0]); close(fd1[1]);
		close(fd2[0]); close(fd2[1]);
		break;
	default: {      /* parent: start the nub */
		Header_T msg = NUB_STARTUP;
		char *s;
		close(fd1[0]);
		close(fd2[1]);
		in = fd2[0];
		out = fd1[1];
		atexit(cleanup);
		if ((s = getenv("TRACE")) != NULL)
			trace = atoi(s);
		tracemsg("server: sending %s\n", msgname(msg));
		send(&msg, sizeof msg);
		send(&state, sizeof state);
		swtch();
		break;
		}
	case 0: {       /* child: run the debugger */
		char arg1[20], arg2[20];
		close(fd1[1]);
		close(fd2[0]);
		sprintf(arg1,  "-in=%d", fd1[0]);
		sprintf(arg2, "-out=%d", fd2[1]);
		execl(cdb, cdb, arg1, arg2, NULL);
		perror(cdb);
		close(fd1[0]);
		close(fd2[1]);
		}
	}
}

void _Cdb_fault(Nub_state_T state) {
	Header_T msg = NUB_FAULT;

	if (out == 0)
		exit(EXIT_FAILURE);
	tracemsg("server: sending %s\n", msgname(msg));
	send(&msg, sizeof msg);
	send(&state, sizeof state);
	swtch();
}
