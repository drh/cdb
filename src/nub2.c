#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "symtab.h"
#include "nub.h"

static char rcsid[] = "$Id$";

extern Nub_callback_T breakhandler;

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

void _Nub_state(Nub_state_T *state) {
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

