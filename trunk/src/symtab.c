#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "arena.h"
#include "table.h"
#include "atom.h"
#include "symtab.h"
#include "nub.h"

static char rcsid[] = "$Id$";

static Arena_T arena;
static Table_T strings;
static Table_T symbols;
static Table_T types;

void _Sym_init(void) {
	if (arena == NULL)
		arena = Arena_new();
	Arena_free(arena);
	if (strings != NULL)
		Table_free(&strings);
	if (symbols != NULL)
		Table_free(&symbols);
	if (types != NULL)
		Table_free(&types);
	types = Table_new(0, 0, 0);
	symbols = Table_new(0, 0, 0);
	strings = Table_new(0, 0, 0);
}

char *_Sym_string(void *key) {
	char *str;

	assert(key);
	str = Table_get(strings, key);
	if (str == NULL) {
		char buf[2048];
		int i = 0, n;
		for (;;) {
			assert(i < sizeof buf - 4);
			n = _Nub_fetch(STR, (char *)key + i, &buf[i], 4);
			assert(n == 4);
			if (buf[i]) i++; else break;
			if (buf[i]) i++; else break;
			if (buf[i]) i++; else break;
			if (buf[i]) i++; else break;
		}
		str = (char *)Atom_string(buf);
		Table_put(strings, key, str);
	}
	return str;
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
		if (sp->name)
			sp->name = _Sym_string(sp->name);
		if (sp->file)
			sp->file = _Sym_string(sp->file);
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
		if (sym->name && strcmp(name, sym->name) == 0)
			return sym;
	}
	return NULL;
}
