#ifndef SYMTAB_INCLUDED
#define SYMTAB_INCLUDED
#include "glue.h"

/* $Id$ */

extern void _Sym_init(struct module *mods[]);
extern const struct ssymbol *_Sym_symbol(const void *sym);
extern const struct stype *_Sym_type(const void *type);
extern const char *_Sym_string(const void *str);
extern struct _Sym_iterator *_Sym_iterator(const void *context);
extern const struct ssymbol *_Sym_next(struct _Sym_iterator *it);

#endif
