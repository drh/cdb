#ifndef SYMTAB_INCLUDED
#define SYMTAB_INCLUDED
#include "glue.h"

/* $Id$ */

extern void _Sym_init(void);
extern struct ssymbol *_Sym_symbol(void *sym);
extern const struct stype *_Sym_type(void *module, int index);
extern const char *_Sym_string(void *module, int index);
extern struct ssymbol *_Sym_find(const char *name, void *context);

#endif
