#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "nub.h"

static char rcsid[] = "$Id";

struct sframe *_Nub_tos;

static Nub_callback_T faulthandler;
static Nub_callback_T breakhandler;
static int frameno;
static struct ssymbol *root;
static struct sframe *fp;
static struct { char *start, *end; } text, data, stack;

static int unpack(union scoordinate w, int *i, short int *x, short int *y) {
	int f;
	static union { int i; char endian; } little = { 1 };

	if (little.endian) {
		*i = w.le.index;
		*x = w.le.x + 1;
		*y = w.le.y;
		f = w.le.flag;
	} else {
		*i = w.be.index;
		*x = w.be.x + 1;
		*y = w.be.y;
		f = w.be.flag;
	}
	return f;
}

static void setcoord(union scoordinate w, struct module *modp, Nub_coord_T *src) {
	int i;

	unpack(w, &i, &src->x, &src->y);
	strncpy(src->file, modp->files[i], sizeof src->file);
}

static void set_state(struct sframe *fp, Nub_state_T *state) {
	strncpy(state->name, fp->func, sizeof state->name);
	setcoord(fp->module->coordinates[fp->ip], fp->module, &state->src);
	state->fp = (void *)fp;
	state->context = fp->tail;
}

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

static int equal(Nub_coord_T *src, union scoordinate w, char *files[]) {
	int i;
	short x, y;

	unpack(w, &i, &x, &y);
	/*
	Two coordinates are `equal' if their
	nondefault components are equal.
	*/
	return (src->y == 0 || src->y == y)
	    && (src->x == 0 || src->x == x)
	    && (src->file[0] == 0 || files[i] && strcmp(src->file, files[i]) == 0);
}

static union scoordinate *src2cp(Nub_coord_T src) {
	int i;

	for (i = 0; _Nub_modules[i]; i++) {
		char **files = _Nub_modules[i]->files;
		union scoordinate *cp = &_Nub_modules[i]->coordinates[1];
		for ( ; cp->i; cp++)
			if (equal(&src, *cp, files))
				return cp;
	}
	return NULL;
}

/* update - update memory maps */
static void update(void) {
	char tos;
#ifdef unix
	{
		extern char etext;
		extern void *sbrk;
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
	int i;

	faulthandler = fault;
	for (i = 0; (m = _Nub_modules[i]) != NULL; i++) {
		struct ssymbol *sp = m->link;
		assert(sp);
		while (sp->uplink)
			sp = sp->uplink;
		if (sp != m->link) {
			sp->uplink = root;
			root = m->link->uplink;
		}
	}
	for (i = 0; _Nub_modules[i]; i++)
		_Nub_modules[i]->link->uplink = root;
	fp = NULL;
	frameno = 0;
	state = z;
	state.context = root;
	update();
	startup(state);
}

void _Nub_bp(int index, struct ssymbol *tail) {
	Nub_state_T state;

	if (!breakhandler)
		return;
	_Nub_tos->ip = index;
	_Nub_tos->tail = tail;
	fp = _Nub_tos;
	fp->up = NULL;
	set_state(fp, &state);
	frameno = 0;
	update();
	breakhandler(state);
}

Nub_callback_T _Nub_set(Nub_coord_T src, Nub_callback_T onbreak) {
	Nub_callback_T prev = breakhandler;
	static union { int i; char c; } x = { 1 };
	union scoordinate *cp = src2cp(src);

	if (cp == NULL)
		return NULL;
	if (onbreak)
		breakhandler = onbreak;
	if (x.c == 1)
		cp->le.flag = 1;
	else
		cp->be.flag = 1;
	return prev;
}

Nub_callback_T _Nub_remove(Nub_coord_T src) {
	static union { int i; char c; } x = { 1 };
	union scoordinate *cp = src2cp(src);

	if (cp == NULL)
		return NULL;
	if (x.c == 1)
		cp->le.flag = 0;
	else
		cp->be.flag = 0;
	return breakhandler;
}

static int valid(const char *address, int nbytes) {
#define xx(z) do { \
	if (address >= z.start && address < z.end) { \
		if (z.end - address < nbytes) \
			return z.end - address; \
		else \
			return nbytes; } } while (0)
	xx(stack);
	xx(data);
	xx(text);
#undef xx
	return 0;
}

int _Nub_fetch(int space, void *address, void *buf, int nbytes) {
	if (nbytes <= 0)
		return 0;
	nbytes = valid(address, nbytes);
	if (nbytes > 0)
		memcpy(buf, address, nbytes);
	return nbytes;
}

int _Nub_store(int space, void *address, void *buf, int nbytes) {
	if (nbytes <= 0)
		return 0;
	nbytes = valid(address, nbytes);
	if (nbytes > 0)
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
	set_state(fp, state);
	return frameno;
}

void _Nub_src(Nub_coord_T src,
	void apply(int i, Nub_coord_T *src, void *cl), void *cl) {
	int i, n = 0;

	assert(apply);
	for (i = 0; _Nub_modules[i]; i++) {
		char **files = _Nub_modules[i]->files;
		union scoordinate *cp = &_Nub_modules[i]->coordinates[1];
		for ( ; cp->i; cp++)
			if (equal(&src, *cp, files)) {
				Nub_coord_T z;
				setcoord(*cp, _Nub_modules[i], &z);
				apply(n, &z, cl);
			}
	}
}

int mAiN(int argc, char *argv[], char *envp[]) {
	extern int main(int, char *[], char *[]);
	extern void _Cdb_startup(Nub_state_T state), _Cdb_fault(Nub_state_T state);
	_Nub_init(_Cdb_startup, _Cdb_fault);
	return main(argc, argv, envp);
}
