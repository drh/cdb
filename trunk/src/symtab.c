/* <symtab.c>=                                                              */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "table.h"
#include "atom.h"
#include "symtab.h"
#include "nub.h"

char *_Sym_string(void *key) {
        char *str;
        static Table_T strings;

        assert(key);
        if (strings == NULL)
                strings = Table_new(0, 0, 0);
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
        static Table_T symbols;

        assert(sym);
        if (symbols == NULL)
                symbols = Table_new(0, 0, 0);
        sp = Table_get(symbols, sym);
        if (sp == NULL) {
                int n;
                NEW(sp);
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
        static Table_T types;

        assert(type);
        if (types == NULL)
                types = Table_new(0, 0, 0);
        ty = Table_get(types, type);
        if (ty == NULL) {
                int n;
                unsigned short len;
                n = _Nub_fetch(TYPE, type, &len, sizeof len);
                assert(n == sizeof len);
                ty = ALLOC(len);
                n = _Nub_fetch(TYPE, type, ty, len);
                assert(n == len);
                Table_put(types, type, ty);
        }
        return ty;
}
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

