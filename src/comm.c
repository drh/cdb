#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "comm.h"

static char rcsid[] = "$Id$";

#ifdef unix
extern int read(int, void *, int);
extern int write(int, const void *, int);
#else
#define read(fd,buf,len) recv(fd,buf,len,0)
#define write(fd,buf,len) send(fd,buf,len,0)
#endif

int trace = 0;
const char *identity = "";

const char *mesgname(Header_T msg) {
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

void tracemesg(const char *fmt, ...) {
	if (trace) {
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		trace--;
	}
}

void recvmesg(SOCKET s, void *buf, int size) {
	int n;

	n = read(s, buf, size);
	tracemesg("%s: received %d bytes\n", identity, n);
	if (n != size)
		tracemesg("%s: **expected %d bytes\n", identity, size);
	assert(n == size);
}

void sendmesg(SOCKET s, const void *buf, int size) {
	int n;

	tracemesg("%s: sending %d bytes\n", identity, size);
	n = write(s, buf, size);
	if (n != size)
		tracemesg("%s: **expected %d bytes\n", identity, size);
	assert(n == size);
}
