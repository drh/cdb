#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nub.h"

static char rcsid[] = "$Id: clientstub.c,v 1.5 1997/06/30 21:42:09 drh Exp $";

static void print(int i, const Nub_coord_T *src, void *cl) {
	static Nub_coord_T prev;

	if (strncmp(prev.file, src->file, sizeof prev.file) != 0) {
		fprintf(stderr, "\n%s:", src->file);
		prev = *src;
	}
	fprintf(stderr, "%d.%d ", src->y, src->x);
}

void _Cdb_startup(Nub_state_T state) {
	int i;
	struct module *m;

	for (i = 0; (m = _Nub_modules[i]) != NULL; i++) {
		int j, k;
		static Nub_coord_T z;
		fprintf(stderr, "module@%p\n", m);
		for (j = 1; m->files[j]; j++)
			fprintf(stderr, " %s", m->files[j]);
		if (m->globals != NULL) {
			const struct ssymbol *p;
			fprintf(stderr, ":");
			for (p = m->globals; p != NULL; p= p->uplink)
				for (j = 1; m->files[j]; j++)
					if (strcmp(p->file, m->files[j]) == 0) {
						fprintf(stderr, " %s %p", p->name, p->type);
						break;
					}
		} else
			fprintf(stderr, "\n");
		for (j = 1; m->files[j]; j++) {
			static Nub_coord_T z;
			strncpy(z.file, m->files[j], sizeof z.file);
			_Nub_src(z, print, NULL);
			fprintf(stderr, "\n");
		}
	}
}

void _Cdb_fault(Nub_state_T state) {
	exit(EXIT_FAILURE);
}
