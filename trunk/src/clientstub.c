#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nub.h"

static char rcsid[] = "$Id$";

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
		fprintf(stderr, "module %x\n", m->uid);
		for (j = 1; m->files[j]; j++)
			fprintf(stderr, " %s", m->files[j]);
		if (m->link != NULL) {
			struct ssymbol *p = m->link;
			fprintf(stderr, ":");
			for (;; ) {
				for (j = 1; m->files[j]; j++)
					if (strcmp(m->constants + p->file, m->files[j]) == 0) {
						fprintf(stderr, " %s %u", m->constants + p->name, p->type);
						break;
					}
				if (p->uplink == 0)
					break;
				p = (struct ssymbol *)(m->constants + p->uplink);
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
