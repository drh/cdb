#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "arena.h"
#include "symtab.h"

static char rcsid[] = "$Id$";

enum { SYM=0 };
static struct mod {
	struct module m;
	void *module;
	char *constants;
	struct mod *link;
} *modules;
static Arena_T arena;

void _Sym_init(struct module *mods[]) {
	if (arena == NULL)
		arena = Arena_new();
	Arena_free(arena);
	modules = NULL;
	if (mods != NULL)
		for (;; mods++) {
			struct module *module;
			struct mod *m;
			int n = _Nub_fetch(SYM, mods, &module, sizeof module);
			assert(n == sizeof module);
			if (module == NULL)
				break;
			m = Arena_alloc(arena, sizeof *m, __FILE__, __LINE__);
			n = _Nub_fetch(SYM, module, &m->m, sizeof m->m);
			assert(n == sizeof m->m);
			m->module = module;
			m->constants = NULL;
			m->link = modules;
			modules = m;
		}
}

static const void *resolve(const char *addr) {
	struct mod *m;

	assert(addr);
	for (m = modules; m != NULL; m = m->link)
		if (addr >= m->m.constants && addr < m->m.constants + m->m.length)
			break;
	assert(m);
	if (m->constants == NULL) {
		int n;
		m->constants = Arena_alloc(arena, m->m.length, __FILE__, __LINE__);
		n = _Nub_fetch(SYM, (void *)m->m.constants, m->constants, m->m.length);
		assert(n == m->m.length);
	}
	return m->constants + (addr - m->m.constants);
}	

const char *_Sym_string(const void *str) {
	return resolve(str);
}

const struct stype *_Sym_type(const void *type) {
	return resolve(type);
}

const struct ssymbol *_Sym_symbol(const void *sym) {
	if (sym == NULL)
		return NULL;
	else
		return resolve(sym);
}

struct _Sym_iterator {
	const struct ssymbol *sym;
	void *module;
	struct mod *m;
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
			while (it->m != NULL && it->m->module == it->module)
				it->m = it->m->link;
			if (it->m != NULL) {
				it->sym = _Sym_symbol(it->m->m.globals);
				it->m = it->m->link;
			} else
				return NULL;
		}
}
