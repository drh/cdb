#ifndef SYMTAB_INCLUDED
#define SYMTAB_INCLUDED
#include "glue.h"

/* $Id$ */

extern void _Sym_init(struct module *mods[]);
extern const struct module *_Sym_module(void *module);
extern const struct ssymbol *_Sym_symbol(void *module, int index);
extern const struct stype *_Sym_type(void *module, int index);
extern const char *_Sym_string(void *module, int index);
extern struct _Sym_iterator *_Sym_iterator(void *context);
extern const struct ssymbol *_Sym_next(struct _Sym_iterator *it);

#endif
