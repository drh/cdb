#ifndef SYMTAB_INCLUDED
#define SYMTAB_INCLUDED
#include "glue.h"
#include "seq.h"
#include "sym.h"

/* $Id$ */

extern Seq_T modules;

extern void _Sym_init(struct module *mods[]);
extern const sym_module_ty _Sym_module(int uname);
extern const sym_symbol_ty _Sym_symbol(int uname, int uid);
extern const sym_type_ty _Sym_type(int uname, int uid);
extern struct _Sym_iterator *_Sym_iterator(int uname, int uid);
extern const sym_symbol_ty _Sym_next(struct _Sym_iterator *it);

#endif
