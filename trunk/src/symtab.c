#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "arena.h"
#include "seq.h"
#include "table.h"
#include "symtab.h"
#include "nub.h"

static char rcsid[] = "$Id$";

static Arena_T arena;
static Table_T modules;
static Seq_T moduleList;

void _Sym_init(struct module *mods[]) {
	if (arena == NULL)
		arena = Arena_new();
	Arena_free(arena);
	if (modules != NULL)
		Table_free(&modules);
	modules = Table_new(0, 0, 0);
	if (moduleList != NULL)
		Seq_free(&moduleList);
	moduleList = Seq_new(0);
	if (mods != NULL) {
		struct module *module;
		int n;
		for (;; mods++) {
			int n = _Nub_fetch(SYM, mods, &module, sizeof module);
			assert(n == sizeof module);
			if (module == NULL)
				break;
			Seq_addhi(moduleList, module);
		}
	}
}

static const void *resolve(void *module, int index) {
	return _Sym_module(module)->constants + index;
}

const struct module *_Sym_module(void *module) {
	struct module *m = Table_get(modules, module);

	if (m == NULL) {
		int n;
		char *constants;
		m = Arena_alloc(arena, sizeof *m, __FILE__, __LINE__);
		n = _Nub_fetch(SYM, module, m, sizeof *m);
		assert(n == sizeof *m);
		constants = Arena_alloc(arena, m->length, __FILE__, __LINE__);
		n = _Nub_fetch(SYM, (void *)m->constants, constants, m->length);
		assert(n == m->length);
		m->constants = constants;
		Table_put(modules, module, m);
	}
	return m;
}

const char *_Sym_string(void *module, int index) {
	return resolve(module, index);
}

const struct stype *_Sym_type(void *module, int index) {
	return resolve(module, index);
}

const struct ssymbol *_Sym_symbol(void *module, int index) {
	if (index == 0)
		return NULL;
	return resolve(module, index);
}

static const struct ssymbol *context2sym(void *context) {
	if (context != NULL) {
		struct ssymbol sym;
		int n = _Nub_fetch(SYM, context, &sym, sizeof sym);
		assert(n == sizeof sym);
		return _Sym_symbol(sym.module, sym.self);
	} else
		return NULL;
}

struct _Sym_iterator {
	const struct ssymbol *sym;
	void *module;
	int i, count;
};

struct _Sym_iterator *_Sym_iterator(void *context) {
	struct _Sym_iterator *it;

	NEW(it);
	it->i = 0;
	it->count = Seq_length(moduleList);
	it->sym = context2sym(context);
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
			it->sym = _Sym_symbol(sym->module, sym->uplink);
			return sym;
		} else if (it->i < it->count) {
			void *module;
			do {
				module = Seq_get(moduleList, it->i);
				it->i++;
				it->sym = context2sym(_Sym_module(module)->link);
			} while (module == it->module);
		} else
			return NULL;
}
