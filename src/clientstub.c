#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nub.h"

static char rcsid[] = "$Id$";

static void print(int i, Nub_coord_T *src, void *cl) {
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
		int j;
		static Nub_coord_T z;
		for (j = 1; m->files[j]; j++)
			fprintf(stderr, " %s", m->files[j]);
		if (m->link->uplink != NULL) {
			struct ssymbol *p;
			fprintf(stderr, ":");
			for (p = m->link->uplink; p; p = p->uplink)
				for (j = 1; m->files[j]; j++)
					if (strcmp(p->file, m->files[j]) == 0) {
						fprintf(stderr, " %s", p->name);
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
