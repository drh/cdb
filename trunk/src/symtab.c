#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "arena.h"
#include "table.h"
#include "seq.h"
#include "atom.h"
#include "symtab.h"
#include "nub.h"

static char rcsid[] = "$Id$";

static Arena_T arena;
static Table_T modules;
static Table_T symbols;
static Table_T types;

void _Sym_init(void) {
	if (arena == NULL)
		arena = Arena_new();
	Arena_free(arena);
	if (modules != NULL)
		Table_free(&modules);
	if (symbols != NULL)
		Table_free(&symbols);
	modules = Table_new(0, 0, 0);
	symbols = Table_new(0, 0, 0);
}

static const char *readConstants(void *module) {
	char *constants;
	struct module m;
	int n;

	n = _Nub_fetch(SYM, module, &m, sizeof m);
	assert(n == sizeof m);
	constants = Arena_alloc(arena, m.length, __FILE__, __LINE__);
	n = _Nub_fetch(SYM, (void *)m.constants, constants, m.length);
	assert(n == m.length);
	return constants;
}

const char *_Sym_string(void *module, int index) {
	const char *constants = Table_get(modules, module);

	if (constants == NULL) {
		constants = readConstants(module);
		Table_put(modules, module, (void *)constants);
	}
	return constants + index;
}

const struct stype *_Sym_type(void *module, int index) {
	const char *constants = Table_get(modules, module);

	if (constants == NULL) {
		constants = readConstants(module);
		Table_put(modules, module, (void *)constants);
	}
	return (void *)(constants + index);
}

struct ssymbol *_Sym_symbol(void *sym) {
	struct ssymbol *sp;

	assert(sym);
	sp = Table_get(symbols, sym);
	if (sp == NULL) {
		int n;
		sp = Arena_alloc(arena, sizeof *sp, __FILE__, __LINE__);
		n = _Nub_fetch(SYM, sym, sp, sizeof *sp);
		assert(n == sizeof *sp);
		Table_put(symbols, sym, sp);
	}
	return sp;
}

struct ssymbol *_Sym_find(const char *name, void *context) {
	struct ssymbol *sym;

	for ( ; context; context = sym->uplink) {
		sym = _Sym_symbol(context);
		if (sym->name && strcmp(name, _Sym_string(sym->module, sym->name)) == 0)
			return sym;
	}
	return NULL;
}
