/* <symstub.c>=                                                             */
#include "symtab.h"
#include <stdlib.h>
#include <string.h>

struct ssymbol *_Sym_symbol(void *sym) { return sym; }
struct stype *_Sym_type(void *type) { return type; }
char *_Sym_string(void *str) { return str; }
/* <symtab functions>=                                                      */
struct ssymbol *_Sym_find(char *name, void *context) {
        while (context) {
                struct ssymbol *sym = _Sym_symbol(context);
                if (sym->name && strcmp(name, sym->name) == 0)
                        return sym;
                context = sym->uplink;
        }
        return NULL;
}


