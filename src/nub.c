#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "nub.h"

static char rcsid[] = "$Id$";

struct sframe *_Nub_tos;

static Nub_callback_T faulthandler;
Nub_callback_T breakhandler;
static int frameno;
static struct sframe *fp;
static struct { char *start, *end; } text, data, stack;

static void movedown(void) {
	if (fp->down) {
		fp->down->up = fp;
		fp = fp->down;
		frameno++;
	}
}

static void moveup(void) {
	if (fp->up) {
		fp = fp->up;
		frameno--;
	}
}

/* update - update memory maps */
static void update(void) {
	char tos;
#ifdef unix
	{
		extern char etext;
		extern void *sbrk(size_t);
		text.start = 0;
		text.end = &etext;
		data.start = &etext;
		data.end = sbrk(0);	
		if (data.end == (char *)-1)
			data.end = data.start;
		stack.start = &tos;
		stack.end = (char *)-1;
	}
#else
	text.start = 0; text.end = (char *)-1;
	data.start = 0; data.end = (char *)-1;
	stack.start = &tos; stack.end = (char *)-1;
#endif
}

void _Nub_init(Nub_callback_T startup, Nub_callback_T fault) {
	Nub_state_T state;
	struct module *m;
	static Nub_state_T z;

	faulthandler = fault;
	fp = NULL;
	frameno = 0;
	state = z;
	state.context = _Nub_modules;
	state.fp = NULL;
	update();
	startup(state);
}

void _Nub_bp(int index) {
	Nub_state_T state;

	if (!breakhandler)
		return;
	_Nub_tos->ip = index;
	fp = _Nub_tos;
	fp->up = NULL;
	state.fp = (char *)fp;
	frameno = 0;
	update();
	breakhandler(state);
}

static int valid(const char *address, int nbytes) {
#define xx(z) if (address >= z.start && address < z.end) \
		      return (unsigned)nbytes > z.end - address ? z.end - address : nbytes
	xx(stack);
	xx(data);
	xx(text);
#undef xx
	return 0;
}

int _Nub_fetch(int space, const void *address, void *buf, int nbytes) {
	if (nbytes <= 0)
		return 0;
	nbytes = valid(address, nbytes);
	assert(nbytes >= 0);
	if (nbytes > 0 && buf)
		memcpy(buf, address, nbytes);
	return nbytes;
}

int _Nub_store(int space, void *address, const void *buf, int nbytes) {
	if (nbytes <= 0)
		return 0;
	if (space == 1) {	/* write _Nub_bpflags[i] */
		int i = (char *)address - (char *)NULL;
		assert(nbytes == 1);
		memcpy(&_Nub_bpflags[i], buf, nbytes);
		return nbytes;
	}
	nbytes = valid(address, nbytes);
	if (nbytes > 0 && buf)
		memcpy(address, buf, nbytes);
	return nbytes;
}

int _Nub_frame(int n, Nub_state_T *state) {
	if (fp == NULL)
		return -1;
	if (n == 0) {
		fp = _Nub_tos;
		frameno = 0;
	} else {
		while (n < frameno && fp->up)
			moveup();
		while (n > frameno && fp->down)
			movedown();
	}
	state->fp = (char *)fp;
	_Nub_state(state);
	return frameno;
}

int mAiN(int argc, char *argv[], char *envp[]) {
	extern int main(int, char *[], char *[]);
	extern void _Cdb_startup(Nub_state_T state), _Cdb_fault(Nub_state_T state);
	_Nub_init(_Cdb_startup, _Cdb_fault);
	return main(argc, argv, envp);
}
