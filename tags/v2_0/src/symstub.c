#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "symtab.h"

static char rcsid[] = "$Id$";

static struct module *dummy[1];
static struct module **modules = dummy;

void _Sym_init(struct module *mods[]) {
	if (mods != NULL)
		modules = mods;
}

const char *_Sym_string(const void *str) { return str; }
const struct stype *_Sym_type(const void *type) { return type; }
const struct ssymbol *_Sym_symbol(const void *sym) { return sym; }

struct _Sym_iterator {
	const struct ssymbol *sym;
	void *module;
	struct module **m;
};

struct _Sym_iterator *_Sym_iterator(const void *context) {
	struct _Sym_iterator *it;

	NEW(it);
	it->sym = _Sym_symbol(context);
	it->m = modules;
	if (it->sym != NULL)
		it->module = it->sym->module;
	else
		it->module = NULL;
	return it;
}

const struct ssymbol *_Sym_next(struct _Sym_iterator *it) {
	assert(it);
	for (;;)
		if (it->sym != NULL) {
			const struct ssymbol *sym = it->sym;
			it->sym = _Sym_symbol(sym->uplink);
			return sym;
		} else {
			while (*it->m != NULL && *it->m == it->module)
				it->m++;
			if (*it->m != NULL) {
				it->sym = _Sym_symbol((*it->m)->globals);
				it->m++;
			} else
				return NULL;
		}
}
