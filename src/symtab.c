#include <assert.h>
#include <stdlib.h>
#include "mem.h"
#include "seq.h"
#include "symtab.h"

static char rcsid[] = "$Id$";

Seq_T pickles, modules;

static void inhale(const char *file) {
	FILE *f = fopen(file, "rb");

	pickles = Seq_new(0);
	if (f != NULL) {
		int c;
		Fmt_fprint(stderr, "inhaling");
		while ((c = fgetc(f)) != EOF) {
			sym_module_ty pickle;
			ungetc(c, f);
			pickle = sym_read_module(f);
			Fmt_fprint(stderr, " %s[%d]", pickle->file, pickle->uname);
			Seq_addhi(pickles, pickle);
		}
		Fmt_fprint(stderr, "\n");
	}
}

void _Sym_init(struct module *mods[]) {
	int i;

	modules = Seq_new(0);
	Fmt_fprint(stderr, "fetching module");
	for (i = 0; ; i++) {
		struct module *m, *mod;
		int n = _Nub_fetch(0, &mods[i], &m, sizeof m);
		assert(n == sizeof m);
		if (m == NULL)
			break;
		NEW(mod);
		n = _Nub_fetch(0, m, mod, sizeof *mod);
		assert(n == sizeof *mod);
		Fmt_fprint(stderr, " [%d]", mod->uname);
		Seq_addhi(modules, mod);
	}
	Fmt_fprint(stderr, "\n");
	inhale("sym.pickle");
}

void *_Sym_address(sym_symbol_ty sym) {
	int i, count = Seq_length(modules);

	assert(sym);
	assert(sym->kind == sym_STATIC_enum);
	for (i = 0; i < count; i++) {
		struct module *m = Seq_get(modules, i);
		if (m->uname == sym->module) {
			void *addr;
			int n = _Nub_fetch(0, m->addresses + sym->v.sym_STATIC.index, &addr, sizeof addr);
			assert(n == sizeof addr);
			return addr;
		}
	}
	return NULL;
}

const sym_module_ty _Sym_module(int uname) {
	int i, count = Seq_length(pickles);

	for (i = 0; i < count; i++) {
		sym_module_ty m = Seq_get(pickles, i);
		if (m->uname == uname)
			return m;
	}
	return NULL;
}

static void *resolve(int uname, int uid) {
	sym_module_ty m = _Sym_module(uname);

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

const sym_symbol_ty _Sym_symbol(int uname, int uid) {
	return resolve(uname, uid);
}

const sym_type_ty _Sym_type(int uname, int uid) {
	return resolve(uname, uid);
}

Seq_T _Sym_visible(sym_symbol_ty sym) {
	int i, count = Seq_length(pickles), uname = 0;
	Seq_T syms = Seq_new(0);

	if (sym != NULL)
		uname = sym->module;
	for ( ; sym != NULL && sym->uplink > 0; sym = _Sym_symbol(sym->module, sym->uplink))
		Seq_addhi(syms, sym);
	for (i = 0; i < count; i++) {
		sym_module_ty pickle = Seq_get(pickles, i);
		if (pickle->uname == uname)
			continue;
		sym = _Sym_symbol(pickle->uname, pickle->globals);
		for ( ; sym != NULL && sym->uplink > 0; sym = _Sym_symbol(sym->module, sym->uplink))
			Seq_addhi(syms, sym);
	}
	return syms;
}
