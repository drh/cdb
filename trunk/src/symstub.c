#include "symtab.h"
#include <stdlib.h>
#include <string.h>

static char rcsid[] = "$Id$";

struct ssymbol *_Sym_symbol(void *sym) { return sym; }
struct stype *_Sym_type(void *type) { return type; }
char *_Sym_string(void *str) { return str; }

struct ssymbol *_Sym_find(const char *name, void *context) {
	struct ssymbol *sym;

	for ( ; context; context = sym->uplink) {
		sym = _Sym_symbol(context);
		if (sym->name && strcmp(name, sym->name) == 0)
			return sym;
	}
	return NULL;
}
