#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "seq.h"
#include "mem.h"
#include "symtab.h"
#include "nub.h"

static char rcsid[] = "$Id$";
#ifndef debug
#define debug(x) (void)0
#endif

static int equal(Nub_coord_T *src, sym_coordinate_ty w)  {
	/*
	Two coordinates are "equal" if their
	nondefault components are equal.
	*/
	return (src->y == 0 || src->y == w->y)
	    && (src->x == 0 || src->x == w->x)
	    && (src->file[0] == 0 || w->file && strcmp(src->file, w->file) == 0);
}

static int coord2bpaddr(Nub_coord_T *src, int *module) {
	int i, count = Seq_length(pickles);

	for (i = 0; i < count; i++) {
		sym_module_ty pickle = Seq_get(pickles, i);
		int j, n = Seq_length(pickle->spoints);
		for (j = 0; j < n; j++) {
			sym_spoint_ty s = Seq_get(pickle->spoints, j);
			if (equal(src, s->src)) {
				*module = pickle->uname;
				count = Seq_length(modules);
				for (i = 0; i < count; i++) {
					struct module *m = Seq_get(modules, i);
					if (pickle->uname == m->uname)
						return j;
				}
				assert(0);
				return 0;
			}
		}
	}
	assert(0);
	return 0;
}

void _Nub_state(Nub_state_T *state, struct sframe *fp) {
	struct sframe frame;
	sym_symbol_ty f;
	sym_module_ty m;
	sym_spoint_ty s;
	int n = _Nub_fetch(0, state->fp, fp ? fp : (fp = &frame), sizeof *fp);

	assert(n == sizeof *fp);
	f = _Sym_symbol(fp->module, fp->func);
	assert(f);
	strncpy(state->name, f->id, sizeof state->name);
	m = _Sym_module(fp->module);
	assert(m);
	s = Seq_get(m->spoints, fp->ip);
	assert(s);
	strncpy(state->src.file, s->src->file, sizeof state->src.file);
	state->src.x = s->src->x; state->src.y = s->src->y;
	state->context = _Sym_symbol(fp->module, s->tail);
}

struct bpoint {
	struct bpoint *link;
	int module, ip;
} *bpoints;
extern Nub_callback_T breakhandler;
static Nub_callback_T bphandler;

static void onbp(Nub_state_T state) {
	struct sframe frame;
	struct bpoint *b;

	_Nub_state(&state, &frame);
	debug(fprintf(stderr, "(break@%s:%d.%d=%d/%d)\n", state.src.file, state.src.y, state.src.x, frame.ip, frame.module));
	for (b = bpoints; b != NULL; b = b->link)
		if (b->ip == frame.ip && b->module == frame.module) {
			(*bphandler)(state);
			break;
		}
}

Nub_callback_T _Nub_set(Nub_coord_T src, Nub_callback_T onbreak) {
	Nub_callback_T prev = bphandler;
	struct bpoint *b;
	int module, ip = coord2bpaddr(&src, &module);
	char flag = 1;
	int n = _Nub_store(1, (char *)NULL + ip, &flag, 1);

	assert(n == 1);
	for (b = bpoints; b != NULL; b = b->link)
		if (b->ip == ip && b->module == module)
			break;
	if (b == NULL) {
		NEW(b);
		b->link = bpoints;
		bpoints = b;
	}
	b->ip = ip;
	b->module = module;
	debug(fprintf(stderr, "(_Nub_set@%s:%d.%d=%d/%d)\n", src.file, src.y, src.x, ip, module));
	bphandler = onbreak;
	breakhandler = onbp;
	return prev;
}

Nub_callback_T _Nub_remove(Nub_coord_T src) {
	int count = 0, module, ip = coord2bpaddr(&src, &module);
	struct bpoint *b, **prev = &bpoints;
 
	for (b = bpoints; b != NULL; b = *prev)
		if (b->ip == ip && b->module == module) {
			*prev = b->link;
			FREE(b);
		} else {
			if (b->ip == ip)
				count++;
			prev = &b->link;
		}
	debug(fprintf(stderr, "(_Nub_remove@%s:%d.%d=%d/%d;%d)\n", src.file, src.y, src.x, ip, module, count));
	if (count == 0) {
		char flag = 0;
		int n = _Nub_store(1, (char *)NULL + ip, &flag, 1);
		assert(n == 1);
	}
	return bphandler;
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
