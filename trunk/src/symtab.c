#include <assert.h>
#include <stdlib.h>
#include "mem.h"
#include "seq.h"
#include "symtab.h"

static char rcsid[] = "$Id$";

Seq_T modules;

static void inhale(const char *file) {
	FILE *f = fopen(file, "r");

	modules = Seq_new(0);
	if (f != NULL) {
		Fmt_fprint(stderr, "inhaling");
		while (!feof(f)) {
			sym_module_ty pickle = sym_read_module(f);
			Fmt_fprint(stderr, " %s[%d]", pickle->file, pickle->uname);
			Seq_addhi(modules, pickle);
		}
		Fmt_fprint(stderr, "\n");
	}
}

void _Sym_init(struct module *mods[]) {
	inhale("sym.pickle");
}

const sym_module_ty _Sym_module(int uname) {
	int i, count = Seq_length(modules);

	for (i = 0; i < count; i++) {
		sym_module_ty m = Seq_get(modules, i);
		if (m->uname == uname)
			return m;
	}
	return NULL;
}

static const void *resolve(int uname, int uid) {
	sym_module_ty m = Seq_module(uname);

	if (m != NULL) {
		int i, count = Seq_length(m->items);
		for (i = 0; i < count; i++) {
			sym_item_ty item = Seq_get(m->items, i);
			if (item->uid == uid)
				switch (item->kind) {
				case sym_Symbol_enum: return item->v.sym_Symbol.symbol;
				case sym_Type_enum:   return item->v.sym_Type.type;
				default: assert(0);
				}
		}
	}
	return NULL;
}

const sym_type_ty _Sym_type(int uname, int uid) {
	return resolve(uname, uid);
}

const struct stype *_Sym_type(int uname, int uid) {
	return resolve(type);
}

struct _Sym_iterator {
	const sym_symbol_ty sym;
	int module, global;
};

struct _Sym_iterator *_Sym_iterator(int uname, int uid) {
	struct _Sym_iterator *it;

	NEW(it);
	it->sym = _Sym_symbol(uname, uid);
	it->module = 0;
	it->global = 0;
	if (it->sym != NULL)
		it->uname = it->sym->module;
	else
		it->uname = 0;
	return it;
}

const sym_symbol_ty _Sym_next(struct _Sym_iterator *it) {
	assert(it);
	for (;;)
		if (it->sym != NULL) {
			const sym_symbol_ty sym = it->sym;
			it->sym = _Sym_symbol(sym->module, sym->uplink);
			return sym;
		} else {
			it->module = 0;
			while (it->m != NULL && it->m->module == it->module)
				it->m = it->m->link;
			if (it->m != NULL) {
				it->sym = _Sym_symbol(it->m->m.globals);
				it->m = it->m->link;
			} else
				return NULL;
		}
}
