/* Go to: <a href="#contents">contents</a>, <a href="#index">index</a><p>   */
/*                                                                          */
/* <h2><a name="ui">User Interface</a></h2>                                 */
/*                                                                          */
/*                                                                          */
/*                                                                          */
/*                                                                          */
/* <symtab.h>=                                                              */
#ifndef SYMTAB_INCLUDED
#define SYMTAB_INCLUDED
#include "glue.h"

extern struct ssymbol *_Sym_symbol(void *sym);
extern struct stype *_Sym_type(void *type);
extern char *_Sym_string(void *str);
extern struct ssymbol *_Sym_find(char *name, void *context);

#endif
