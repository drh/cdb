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

void _Sym_init() {
	if (arena == NULL)
		arena = Arena_new();
	Arena_free(arena);
	if (modules != NULL)
		Table_free(&modules);
	if (symbols != NULL)
		Table_free(&symbols);
	if (types != NULL)
		Table_free(&types);
	modules = Table_new(0, 0, 0);
	types = Table_new(0, 0, 0);
	symbols = Table_new(0, 0, 0);
}

static Seq_T readConstants(void *module) {
	Seq_T constants = Seq_new(0);
	struct module m;
	char *cnsts;
	int i = 0, n;

	n = _Nub_fetch(SYM, module, &m, sizeof m);
	assert(n == sizeof m);
	cnsts = Arena_alloc(arena, m.length, __FILE__, __LINE__);
	n = _Nub_fetch(SYM, (void *)m.constants, cnsts, m.length);
	assert(n == m.length);
	for (n = 0; n < m.length; n++, i++) {
		struct constant *p = (struct constant *)(cnsts + n);
		switch (p->tag) {
		case cString:
			tracemsg("constants[%d]=\"%s\"\n", i, p->u.s.str);
			n += p->u.s.len;
			break;
		default: assert(0);
		}
		Seq_addhi(constants, p);
	}
	return constants;
}

const char *_Sym_string(void *module, int index) {
	struct constant *p;
	Seq_T constants = Table_get(modules, module);

	if (constants == NULL) {
		constants = readConstants(module);
		Table_put(modules, module, constants);
	}
	p = Seq_get(constants, index);
	assert(p->tag == cString);
	return p->u.s.str;
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

struct stype *_Sym_type(void *type) {
	struct stype *ty;

	assert(type);
	ty = Table_get(types, type);
	if (ty == NULL) {
		int n;
		unsigned short len;
		n = _Nub_fetch(TYPE, type, &len, sizeof len);
		assert(n == sizeof len);
		ty = Arena_alloc(arena, len, __FILE__, __LINE__);
		n = _Nub_fetch(TYPE, type, ty, len);
		assert(n == len);
		Table_put(types, type, ty);
	}
	return ty;
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
