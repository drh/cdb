#ifndef SYMTAB_INCLUDED
#define SYMTAB_INCLUDED
#include "glue.h"

/* $Id$ */
extern struct ssymbol *_Sym_symbol(void *sym);
extern struct stype *_Sym_type(void *type);
extern char *_Sym_string(void *str);
extern struct ssymbol *_Sym_find(const char *name, void *context);

#endif
