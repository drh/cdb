#ifndef SYMTAB_INCLUDED
#define SYMTAB_INCLUDED
#include "glue.h"
#include "seq.h"
#include "sym.h"

/* $Id$ */

extern Seq_T modules, pickles;

extern void _Sym_init(struct module *mods[]);
extern void *_Sym_address(sym_symbol_ty sym);
extern const sym_symbol_ty _Sym_lookup(const char *file, const char *name, sym_symbol_ty sym);
extern const sym_module_ty _Sym_module(int uname);
extern const sym_symbol_ty _Sym_symbol(int uname, int uid);
extern const sym_type_ty _Sym_type(int uname, int uid);
extern Seq_T _Sym_visible(sym_symbol_ty sym);

#endif
