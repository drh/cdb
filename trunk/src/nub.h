#ifndef NUB_INCLUDED
#define NUB_INCLUDED
#include "glue.h"

/* $Id$ */

typedef struct {
	char file[32];
	unsigned short x, y;
} Nub_coord_T;

typedef struct {
	char name[32];
	Nub_coord_T src;
	char *fp;
	void *context;
} Nub_state_T;

typedef void (*Nub_callback_T)(Nub_state_T state);

enum { SYM, TYPE, STR };

extern struct module *_Nub_modules[];
extern struct sframe *_Nub_tos;

extern void _Nub_init(Nub_callback_T startup, Nub_callback_T fault);
extern void _Nub_bp(int index, struct ssymbol *tail);
extern void _Nub_src(Nub_coord_T src,
	void apply(int i, Nub_coord_T *src, void *cl), void *cl);
extern Nub_callback_T _Nub_set(Nub_coord_T src, Nub_callback_T onbreak);
extern Nub_callback_T _Nub_remove(Nub_coord_T src);
extern int _Nub_fetch(int space, void *address, void *buf, int nbytes);
extern int _Nub_store(int space, void *address, void *buf, int nbytes);
extern int _Nub_frame(int n, Nub_state_T *state);
#endif
