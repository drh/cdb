#ifndef SYMTAB_INCLUDED
#define SYMTAB_INCLUDED
#include "glue.h"

/* $Id: symtab.h,v 1.7 1997/06/27 00:59:25 drh Exp $ */

extern void _Sym_init(struct module *mods[]);
extern const struct ssymbol *_Sym_symbol(const void *sym);
extern const struct stype *_Sym_type(const void *type);
extern const char *_Sym_string(const void *str);
extern struct _Sym_iterator *_Sym_iterator(const void *context);
extern const struct ssymbol *_Sym_next(struct _Sym_iterator *it);

#endif
