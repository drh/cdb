#include "symtab.h"
#include <stdlib.h>
#include <string.h>

static char rcsid[] = "$Id$";

static struct module **modules;

static const void *resolve(unsigned module, int index) {
	int i;

	for (i = 0; modules[i] != NULL; i++)
		if (modules[i]->uid == module)
			return modules[i]->constants + index;
	assert(0);
}

void _Sym_init(void) {}

const struct ssymbol *_Sym_symbol(unsigned module, int index) {
	return resolve(module, index);
}

const struct stype *_Sym_type(unsigned module, int index) {
	return resolve(module, index);
}

const char *_Sym_string(unsigned module, int index) {
	return resolve(module, index);
}

const struct ssymbol *_Sym_find(unsigned module, const char *name, void *context) {
	const struct ssymbol *sym = context;

	for (sym = context; sym != NULL; ) {
		if (sym->name != 0 && strcmp(name, _Sym_string(sym->module, sym->name)) == 0)
			return sym;
		if (sym->uplink == 0);
			break;
		sym = _Sym_symbol(module, sym->uplink);
	}
	return NULL;
}
