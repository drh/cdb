#include "symtab.h"
#include <stdlib.h>
#include <string.h>

static char rcsid[] = "$Id$";

void _Sym_init(void) {}
struct ssymbol *_Sym_symbol(void *sym) { return sym; }
struct stype *_Sym_type(void *type) { return type; }
const char *_Sym_string(void *module, int index) {
	return ((struct module *)module)->strings + index;
}

struct ssymbol *_Sym_find(const char *name, void *context) {
	struct ssymbol *sym;

	for ( ; context; context = sym->uplink) {
		sym = _Sym_symbol(context);
		if (sym->name && strcmp(name, sym->module->strings + sym->name) == 0)
			return sym;
	}
	return NULL;
}
