/* <HEADER>                                                                 */
/* <title>cdb - A Machine Independent Debugger</title></HEADER>             */
/* <BODY>                                                                   */
/*                                                                          */
/* <H1>cdb - A Machine Independent Debugger</H1>                            */
/* <address>David R. Hanson and Mukund Raghavachari<br>                     */
/* Dept. of Computer Science, Princeton University<br>                      */
/* Princeton, NJ 08544                                                      */
/* </address>                                                               */
/*                                                                          */
/* <h2><a name="contents">Contents</a></h2>                                 */
/* <ul>                                                                     */
/* <li><a href="#intro">Introduction</a>                                    */
/* <li><a href="#stab">Emitting Symbol Tables</a>                           */
/* <li><a href="#code">Emitting Debugging Code</a>                          */
/* <li><a href="#interface">Nub Interface</a>                               */
/* <li><a href="#implementation">Nub Implementation</a>                     */
/* <li><a href="#ui">User Interface</a>                                     */
/* <li><a href="#index">Index</a>                                           */
/* </ul>                                                                    */
/*                                                                          */
/* <h2><a name="intro">Introduction</a></h2>                                */
/*                                                                          */
/* There are 4 pieces in the [[cdb]] puzzle:                                */
/* <ol>                                                                     */
/* <li>Machine-independent [[cdb]] symbol tables, emitted at compile time.  */
/* <li>The interface to the debugging `nub,' which implements a set         */
/* of machine-independent debugging functions.                              */
/* <li>The implementation of the nub.                                       */
/* <li>A user interface to a debugger that calls the nub functions          */
/* in response to user requests and program events, like breakpoints.       */
/* </ol>                                                                    */
/*                                                                          */
/* The symbol tables are the data that the nub needs to implement           */
/* its functions. Although the symbol tables are machine independent,       */
/* the nub interface hides the details of their representation behind       */
/* a procedural interface to permit different implementations of            */
/* the nub. The implementation described here is purely functional one      */
/* in which the client debugger shares the same address space               */
/* as the debuggee. But other nub implementations are possible; for example, */
/* the nub can be implemented as a network server, so that the debugger     */
/* can run in a different address space. The nub itself, however,           */
/* must have access to the address space of the debuggee.<p>                */
/*                                                                          */
/* Go to: <a href="#contents">contents</a>, <a href="#index">index</a><p>   */
/*                                                                          */
/* <h2><a name="stab">Symbol Table Emitter</a></h2>                         */
/*                                                                          */
/* Symbol tables are emitted by a set of `stab' routines that [[lcc]]       */
/* calls at strategic points current compilation. Before and after each functions, */
/* before each source line, and before and after each block are examples    */
/* of such points.<p>                                                       */
/*                                                                          */
/* [[cdb]] symbol tables are emitted by the functions in [[stab.c]]:        */
/*                                                                          */
/* <stab.c>=                                                                */
#define NDEBUG
#include "c.h"
#undef NDEBUG
#include "glue.h"
#include <stdio.h>
#include <stddef.h>
#include <time.h>

static char rcsid[] = "$Id$";

/* <stab data>=                                                             */
static char *leader;

/* <stab data>=                                                             */
static int maxalign;
/* <stab data>=                                                             */
static List typelist;
/* [[ltov]] converts a [[List]] of pointers to a null-terminated            */
/* array of pointers (see                                                   */
/* <a href="http://www.cs.princeton.edu/software/lcc/lcc/src/list.c">[[src/list.c]]</a>).<p> */
/*                                                                          */
/* Variables like [[ntypes]] and [[nstrings]] keep track of the number of bytes */
/* emitted for each component of the symbol tables, and are incremented     */
/* at the appropriate points, as shown above.                               */
/*                                                                          */
/* <stab data>=                                                             */
static int ntypes;
static int ncoordinates;
static int nfiles;
static int nsymbols;
static int nmodules;
static int nstrings;
/* <h3>Modules</h3>                                                         */
/*                                                                          */
/* Modules are easy to generate, because they're just a sequence of pointers. */
/* The name of the global variable for a module must be different than      */
/* all other modules in the program. This name is generated                 */
/* from the current time of day and the process id:                         */
/*                                                                          */
/* <stab data>=                                                             */
static Symbol module;

/* [[cpp]] and [[sp]] are pointers to null-terminated                       */
/* arrays of pointers to source coordinates                                 */
/* and symbols that [[lcc]] has collected during compilation. [[sp[i]]] is  */
/* the `tail' of the symbol-table at the source coordinate given by [[cpp[i]]]. */
/* That is, [[sp[i]]] points to a list of symbols visible at the execution  */
/* point given by [[cpp[i]]]. [[symroot]] is the list of top-level symbols. */
/* Both of these lists are threaded through the symbols' [[up]] fields.     */
/* [[cp]] is the last source coordinate in the input,                       */
/* and [[ignore]] is always null.<p>                                        */
/*                                                                          */
/* <h3>Files</h3>                                                           */
/*                                                                          */
/* More than one file can contribute to a module, so [[stab.c]] keeps track */
/* of up to 31 files and their nonzero indices into the [[files]] array:    */
/*                                                                          */
/* <stab data>=                                                             */
static char *filelist[32+1];
/* <stab data>=                                                             */
static int epoints;
static List coordlist;
/* These lists are emitted at the end of compilation as an array of execution points */
/* by [[stabend]]. [[coordinates]] is the symbol-table entry for this array: */
/*                                                                          */
/* <stab data>=                                                             */
static Symbol coordinates;

/* [[address]] is the address of a global or static variable,               */
/* and [[offset]] is the offset from                                        */
/* the shadow stack frame to a local variable or                            */
/* a parameter. This offset is computed on the first call to the function   */
/* in which the local or parameter is declared.                             */
/* [[scope]] and [[sclass]] have the same values as in [[Symbol]]s,         */
/* and [[type]] to the [[stype]] for the variable.                          */
/* [[uplink]] points the [[ssymbol]] structure for another identifier       */
/* in the same or an enclosing scope.<p>                                    */
/*                                                                          */
/* [[stabend]] emits the [[ssymbol]] structures by emitting the symbols     */
/* in the [[sp]] array and the link symbol:                                 */
/*                                                                          */
/* <stab data>=                                                             */
static List symbollist;
static Symbol link;

/* [[point_hook]] changes [[*e]] to IR that's equivalent to the C expression */
/* <dl>                                                                     */
/* <dd>[[(module.coordinates[i].le.flag && _Nub_bp(i, tail), *e)]]          */
/* </dl>                                                                    */
/* [[module]] is the generated variable that holds the module's             */
/* symbol-table data, [[i]] is the index in the array of source coordinates */
/* of the current execution point, and [[_Nub_bp]] is the nub's breakpoint handler. */
/*                                                                          */
/*                                                                          */
/* <stab data>=                                                             */
static Symbol nub_bp;

/* <stab data>=                                                             */
static Symbol nub_tos;
/* <stab data>=                                                             */
static Symbol tos;

/* <stab prototypes>=                                                       */
static void comment(char *fmt, ...);
/* <stab prototypes>=                                                       */
static int pad(int align, int lc);
/* <stab prototypes>=                                                       */
static int emit_string(int lc, char *str);
/* <stab prototypes>=                                                       */
static int fileindex(char *name);
/* <stab prototypes>=                                                       */
static Symbol emit_ssymbol(Symbol p);
/* <stab prototypes>=                                                       */
static Symbol tail(void);

/* [[comment]] emits an assembly language comment, which helps identify     */
/* the various pieces of the symbol-table output:                           */
/*                                                                          */
/* <stab functions>=                                                        */
static void comment(char *fmt, ...) {
        va_list ap;

        print(leader);
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
}

/* [[]] emits an initialization for the type                                */
/* given by its second argument:                                            */
/*                                                                          */
/* <stab functions>=                                                        */
static int emit_value(int lc, Type ty, ...) {
        Value v;
        va_list ap;

        va_start(ap, ty);
        if (lc == 0)
                maxalign = 0;
        lc = pad(ty->align, lc);
        switch (ty->op) {
        case INT:      v.i = va_arg(ap, long);          break;
        case UNSIGNED: v.u = va_arg(ap, unsigned long); break;
        case FLOAT:    v.d = va_arg(ap, long double);   break;
        case POINTER:
                defpointer(va_arg(ap, Symbol));
                return lc + ty->size;
        default: assert(0);
        }
        va_end(ap);
        (*IR->defconst)(ty->op, ty->size, v);
        return lc + ty->size;
}

/* [[emit_value]] calls [[pad]], which                                      */
/* emits padding, when necessary, so that the value will appear at the proper offset. */
/* [[lc]] in the code above tracks this offset.                             */
/*                                                                          */
/* <stab functions>=                                                        */
static int pad(int align, int lc) {
        assert(align);
        if (lc%align) {
                (*IR->space)(align - lc%align);
                lc = roundup(lc, align);
        }
        if (align > maxalign)
                maxalign = align;
        return lc;
}

/* As this code suggests, the array is terminated by null name value.       */
/* [[emit_string]] emits a pointer to a string constant:                    */
/*                                                                          */
/* <stab functions>=                                                        */
static int emit_string(int lc, char *str) {
        if (str) {
                Symbol p = mkstr(str);
                nstrings += p->type->size;
                return emit_value(lc, voidptype, p->u.c.loc);
        } else
                return emit_value(lc, voidptype, NULL);
}

/* Before a type can be emitted, it must be annotated with a                */
/* symbol to label its emitted [[stype]] structure. As compilation          */
/* progresses, other parts of [[stab.c]] annotate types and append them     */
/* to [[typelist]] by calling [[annotate]], which traverses the type,       */
/* generates the necessary symbol, and stores it in the type's undocumented [[x.xt]] field, */
/* which also serves to mark the type as annotated:                         */
/*                                                                          */
/* <stab functions>=                                                        */
static Symbol annotate(Type ty) {
        if (ty->x.xt == NULL) {
                ty->x.xt = genident(STATIC, array(unsignedtype, 0, 0), GLOBAL);
                typelist = append(ty, typelist);
                /* [[append]] is one of [[lcc]]'s list-manipulation functions               */
                /* (see Exercise 2.15 and                                                   */
                /* <a href="http://www.cs.princeton.edu/software/lcc/lcc/src/list.c">[[src/list.c]]</a>).<p> */
                /*                                                                          */
                /* [[annotate]]'s bottom-up traversal labels the type's operands, if there are any: */
                /*                                                                          */
                /* <traverse ty>=                                                           */
                switch (ty->op) {
                case CONST: case VOLATILE: case CONST+VOLATILE:
                case POINTER: case ARRAY:
                        annotate(ty->type);
                        break;
                case STRUCT: case UNION: {
                        Field f;
                        for (f = fieldlist(ty); f; f = f->link)
                                annotate(f->type);
                        break;
                        }
                case FUNCTION: {
                        int i;
                        annotate(ty->type);
                        if (ty->u.f.proto)
                                for (i = 0; ty->u.f.proto[i]; i++)
                                        annotate(ty->u.f.proto[i]);
                        break;
                        }
                }
        }
        return ty->x.xt;
}

/* When [[lcc]] is compiled for use with [[cdb]], symbols are extended      */
/* with a [[y]] field, which has the type                                   */
/* <pre>struct {                                                            */
/*         Symbol p;                                                        */
/*         unsigned emitted:1;                                              */
/*         unsigned short pt[2];                                            */
/* };                                                                       */
/* </pre>                                                                   */
/* [[y.p]] is the symbol-table entry for the symbol's                       */
/* emitted [[ssymbol]] structure (see below).                               */
/*                                                                          */
/* [[coordinates]] and [[files]] are symbol-table                           */
/* entries for the corresponding initialized variables.                     */
/* [[stab.c]]'s [[stabend]] function is called at the end of compilation,   */
/* and it emits the definitions of these variables.                         */
/*                                                                          */
/* <stab functions>=                                                        */
static void stabend(Coordinate *cp, Symbol symroot,
        Coordinate *cpp[], Symbol sp[], Symbol *ignore) {
        Symbol types, files;

        /* <emit symbols>=                                                          */
        {
                Symbol p, *allsyms = ltov(&symbollist, PERM);
                int i;
                for (i = 0; allsyms[i]; i++)
                        emit_ssymbol(allsyms[i]);
                p = emit_ssymbol(symroot);
                /* <emit the link>=                                                         */
                {
                        int lc;
                        comment("the link symbol\n");
                        defglobal(link, DATA);
                        lc = emit_value(0, inttype, 0L);
                        lc = emit_value(lc, voidptype, NULL);
                        lc = emit_value(lc, voidptype, NULL);
                        lc = emit_value(lc, voidptype, NULL);
                        lc = emit_value(lc, unsignedchar, 0L);
                        lc = emit_value(lc, unsignedchar, 0L);
                        lc = emit_value(lc, voidptype, NULL);
                        lc = emit_value(lc, voidptype, p->y.p);
                        lc = pad(maxalign, lc);
                        nsymbols += lc;
                }
        }
        /* [[stabend]] emits the coordinates by walking down the parallel           */
        /* [[cpp]] and [[sp]] arrays:                                               */
        /*                                                                          */
        /* <emit coordinates>=                                                      */
        {
                int i, lc;
                Coordinate **cpp = ltov(&coordlist, PERM);
                comment("coordinates:\n");
                defglobal(coordinates, DATA);
                /* <emit null coordinate>=                                                  */
                lc = emit_value(0, unsignedtype, 0UL);
                lc = pad(maxalign, lc);
                for (i = 0; cpp[i]; i++)
                        /* [[flag]] is one if a breakpoint is set at the execution point,           */
                        /* and [[index]] is the index of the file name in the [[files]]             */
                        /* array. The union is used to emit each coordinate as an unsigned integer  */
                        /* on either big endians or little endians.                                 */
                        /*                                                                          */
                        /* <emit coordinate cpp[i]>=                                                */
                        {
                                static int n;
                                Coordinate *cp = cpp[i];
                                union scoordinate w;
                                /* <pack *cp into w>=                                                       */
                                w.i = 0;
                                if (IR->little_endian) {
                                        w.le.index = fileindex(cp->file);
                                        w.le.x = cp->x;
                                        w.le.y = cp->y;                                 
                                } else {
                                        w.be.index = fileindex(cp->file);
                                        w.be.x = cp->x;
                                        w.be.y = cp->y;
                                }                               
                                comment("%d: %s(%d) %d.%d\n", ++n, cp->file ? cp->file : "",
                                        fileindex(cp->file), cp->y, cp->x);
                                lc = emit_value(0, unsignedtype, (unsigned)w.i);
                                lc = pad(maxalign, lc);
                                ncoordinates += lc;
                        }
                /* <emit null coordinate>=                                                  */
                lc = emit_value(0, unsignedtype, 0UL);
                lc = pad(maxalign, lc);
                ncoordinates += lc;
        }

        /* The [[files]] array is emitted by simply emitting the pointers           */
        /* in [[filelist]]:                                                         */
        /*                                                                          */
        /* <emit files>=                                                            */
        {
                int i = 0, lc;
                files = genident(STATIC, array(ptr(chartype), 1, 0), GLOBAL);
                comment("files:\n");
                defglobal(files, DATA);
                /* <emit NULL>=                                                             */
                lc = emit_value(0, voidptype, NULL);
                lc = pad(maxalign, lc);
                nfiles += lc;
                for (i = 1; filelist[i]; i++) {
                        lc = emit_string(0, filelist[i]);
                        lc = pad(maxalign, lc);
                        nfiles += lc;
                }
                /* <emit NULL>=                                                             */
                lc = emit_value(0, voidptype, NULL);
                lc = pad(maxalign, lc);
                nfiles += lc;
        }

        /* [[mksymbol]] allocates an unattached symbol-table entry and              */
        /* initializes it with the storage class, name, and type passed as arguments (see */
        /* <a href="http://www.cs.princeton.edu/software/lcc/lcc/src/sym.c">[[src/sym.c]]</a>). */
        /* All that remains is to define the module variable and emit its initialization: */
        /*                                                                          */
        /* <emit module>=                                                           */
        {
                int lc;
                comment("module:\n");
                defglobal(module, DATA);
                lc = emit_value( 0, voidptype, coordinates);
                lc = emit_value(lc, voidptype, files);
                lc = emit_value(lc, voidptype, link);
                lc = pad(maxalign, lc);
                nmodules += lc;
        }
        /* For structures and unions, [[annotate]] makes a pass over the field list */
        /* annotating the type for each field.                                      */
        /* [[fieldlist]] returns a list of fields threaded                          */
        /* through [[lcc]]'s field structures (see                                  */
        /* <a href="http://www.cs.princeton.edu/software/lcc/lcc/src/types.c">[[src/types.c]]</a>). */
        /* For functions, [[annotate]] runs over the arguments.<p>                  */
        /*                                                                          */
        /* At the end of compilation, [[stabend]] emits all the types on [[typelist]]: */
        /*                                                                          */
        /* <emit types>=                                                            */
        {
                Type *alltypes = ltov(&typelist, PERM), ty;
                int i;
                for (i = 0; (ty = alltypes[i]) != NULL; i++) {
                        /* [[stype]] structures are a prefix rendition                              */
                        /* of [[lcc]]'s internal representation (see Chapter 4).                    */
                        /* [[len]] is the number of bytes in the [[stype]]                          */
                        /* structure itself.                                                        */
                        /* [[op]] is the type operator, and [[size]] is the size in bytes of objects */
                        /* of the type. The union [[u]] holds type-specific fields.                 */
                        /* Given an [[lcc]] [[Type]], [[stab.c]] emits an [[stype]] structure:      */
                        /*                                                                          */
                        /* <emit ty>=                                                               */
                        struct stype stype;
                        int lc;
                        comment("%t\n", ty);
                        assert(ty->x.xt);
                        defglobal(ty->x.xt, DATA);
                        switch (ty->op) {
                        case VOLATILE: case CONST+VOLATILE:
                        case CONST:     /* <emit a qualified type>=                                                 */
                                        {
                                                unsigned len = sizeof stype.u.q;
                                                /* <emit len, op, and size>=                                                */
                                                lc = emit_value( 0, unsignedshort, (unsigned long)offsetof(struct stype, u) + len);
                                                lc = emit_value(lc, unsignedchar,  (unsigned long)ty->op);
                                                lc = emit_value(lc, unsignedtype,  (unsigned long)ty->size);
                                                lc = emit_value(lc, voidptype, ty->type->x.xt);
                                        }  break;
                        case POINTER:   /* <emit pointer type ty>=                                                  */
                                        {
                                                unsigned len = sizeof stype.u.p;
                                                /* <emit len, op, and size>=                                                */
                                                lc = emit_value( 0, unsignedshort, (unsigned long)offsetof(struct stype, u) + len);
                                                lc = emit_value(lc, unsignedchar,  (unsigned long)ty->op);
                                                lc = emit_value(lc, unsignedtype,  (unsigned long)ty->size);
                                                lc = emit_value(lc, voidptype, ty->type->x.xt);
                                        }
   break;
                        case ARRAY:     /* <emit array type ty>=                                                    */
                                        {
                                                unsigned len = sizeof stype.u.a;
                                                /* <emit len, op, and size>=                                                */
                                                lc = emit_value( 0, unsignedshort, (unsigned long)offsetof(struct stype, u) + len);
                                                lc = emit_value(lc, unsignedchar,  (unsigned long)ty->op);
                                                lc = emit_value(lc, unsignedtype,  (unsigned long)ty->size);
                                                lc = emit_value(lc, voidptype, ty->type->x.xt);
                                                if (ty->type->size > 0)
                                                        lc = emit_value(lc, unsignedtype,
                                                                (unsigned long)ty->size/ty->type->size);
                                                else
                                                        lc = emit_value(lc, unsignedtype, 0UL);
                                        }     break;
                        case FUNCTION:  /* <emit function type ty>=                                                 */
                                        {
                                                unsigned len = sizeof stype.u.f.type;
                                                if (ty->u.f.proto && ty->u.f.proto[0]) {
                                                        Type *tp = ty->u.f.proto;
                                                        for ( ; *tp; tp++)
                                                                len += sizeof stype.u.f.args;
                                                }
                                                /* <emit len, op, and size>=                                                */
                                                lc = emit_value( 0, unsignedshort, (unsigned long)offsetof(struct stype, u) + len);
                                                lc = emit_value(lc, unsignedchar,  (unsigned long)ty->op);
                                                lc = emit_value(lc, unsignedtype,  (unsigned long)ty->size);
                                                lc = emit_value(lc, voidptype, ty->type->x.xt, lc);
                                                if (ty->u.f.proto && ty->u.f.proto[0]) {
                                                        Type *tp = ty->u.f.proto;
                                                        for ( ; *tp; tp++)
                                                                lc = emit_value(lc, voidptype, (*tp)->x.xt);
                                                }
                                        }  break;
                        case STRUCT:
                        case UNION:     /* <emit aggregate type ty>=                                                */
                                        {
                                                unsigned len = sizeof stype.u.s.tag;
                                                Field f;
                                                for (f = fieldlist(ty); f; f = f->link)
                                                        len += sizeof stype.u.s.fields;
                                                /* <emit len, op, and size>=                                                */
                                                lc = emit_value( 0, unsignedshort, (unsigned long)offsetof(struct stype, u) + len);
                                                lc = emit_value(lc, unsignedchar,  (unsigned long)ty->op);
                                                lc = emit_value(lc, unsignedtype,  (unsigned long)ty->size);
                                                lc = emit_string(lc, ty->u.sym->name);
                                                for (f = fieldlist(ty); f; f = f->link) {
                                                        lc = emit_string(lc, f->name);
                                                        lc = emit_value(lc, voidptype, f->type->x.xt);
                                                        if (f->lsb) {
                                                                union offset o;
                                                                if (IR->little_endian) {
                                                                        o.le.size = -fieldsize(f) + 1;
                                                                        o.le.lsb = fieldright(f);
                                                                        o.le.offset = f->offset;
                                                                } else {
                                                                        o.be.size = -fieldsize(f) + 1;
                                                                        o.be.lsb = fieldright(f);
                                                                        o.be.offset = f->offset;
                                                                }
                                                                lc = emit_value(lc, unsignedtype, o.offset);
                                                        } else
                                                                lc = emit_value(lc, unsignedtype, f->offset);
                                                }
                                        } break;
                        case ENUM:      /* <emit enum type ty>=                                                     */
                                        {
                                                Symbol *p;
                                                unsigned len = sizeof stype.u.e.tag;
                                                for (p = ty->u.sym->u.idlist; *p; p++)
                                                        len += sizeof stype.u.e.enums;
                                                /* <emit len, op, and size>=                                                */
                                                lc = emit_value( 0, unsignedshort, (unsigned long)offsetof(struct stype, u) + len);
                                                lc = emit_value(lc, unsignedchar,  (unsigned long)ty->op);
                                                lc = emit_value(lc, unsignedtype,  (unsigned long)ty->size);
                                                lc = emit_string(lc, ty->u.sym->name);
                                                for (p = ty->u.sym->u.idlist; *p; p++) {
                                                        lc = emit_string(lc, (*p)->name);
                                                        lc = emit_value(lc, inttype, (*p)->u.value);
                                                }
                                        }      break;
                        case VOID: case FLOAT: case INT:
                        case UNSIGNED:  /* [[pad]] keeps track of the maximum alignment in [[maxalign]],            */
                                        /* and [[maxalign]] is used to emit padding at the end of a structure,      */
                                        /* as suggested above.                                                      */
                                        /* [[emit_value]] resets [[maxalign]] to zero whenever it emits a new structure, */
                                        /* which is when its [[lc]] argument is zero.<p>                            */
                                        /*                                                                          */
                                        /* [[defpointer]] initializes a pointer to the address                      */
                                        /* of a global variable or to the null pointer (see                         */
                                        /* <a href="http://www.cs.princeton.edu/software/lcc/lcc/src/init.c">[[src/init.c]]</a>)).<p> */
                                        /*                                                                          */
                                        /*                                                                          */
                                        /* <emit scalar type ty>=                                                   */
                                        {
                                                unsigned len = 0;
                                                /* <emit len, op, and size>=                                                */
                                                lc = emit_value( 0, unsignedshort, (unsigned long)offsetof(struct stype, u) + len);
                                                lc = emit_value(lc, unsignedchar,  (unsigned long)ty->op);
                                                lc = emit_value(lc, unsignedtype,  (unsigned long)ty->size);
                                        }
    break;
                        default:assert(0);
                        }
                        lc = pad(maxalign, lc);
                        ntypes += lc;
                }
        }
        /* [[stab.c]] reports the amount of space required for its symbol table:    */
        /*                                                                          */
        /* <report statistics>=                                                     */
        #define printit(x) sprintf(buf, "%6d", n##x); \
                fprint(stderr, "%s " #x "\n", buf); total += n##x
        {
                char buf[100];
                int total = 0;
                printit(types);
                printit(coordinates);
                printit(files);
                printit(symbols);
                printit(strings);
                printit(modules);
                sprintf(buf, "%6d", total);
                fprint(stderr, "%s bytes total\n", buf);
        }
        #undef printit
}
/* The elements of [[filelist]] are the symbol-table entries                */
/* for the string literals that hold the file names.                        */
/* [[fileindex]] returns the index in [[filelist]] of a given file name,    */
/* adding it if necessary:                                                  */
/*                                                                          */
/* <stab functions>=                                                        */
static int fileindex(char *name) {
        int i;

        if (name == NULL)
                return 0;
        for (i = 1; filelist[i]; i++)
                if (filelist[i] == name)
                        return i;
        if (i >= NELEMS(filelist)-1)
                return 0;
        filelist[i] = name;
        return i;
}

/* <stab functions>=                                                        */
static Symbol emit_ssymbol(Symbol p) {
        while (p && /* [[emit_symbol]] is like [[annotate]]: it emits an initialized [[ssymbol]] */
                    /* for its argument, and returns the symbol-table entry for that initialized */
                    /* variable. [[emit_symbol]] annotates its argument with the symbol-table   */
                    /* entry for the generated variable, and this annotation marks the symbol   */
                    /* as having been emitted:                                                  */
                    /*                                                                          */
                    /*                                                                          */
                    /* <p is external>=                                                         */
                      (!p->defined
                    && (p->sclass == EXTERN || isfunc(p->type) && p->sclass == AUTO))
)
                p = p->up;
        if (p == NULL)
                return NULL;
        if (!p->y.emitted) {
                int lc;
                Symbol up;
                p->y.emitted = 1;
                /* <emit symbol p>=                                                         */
                up = emit_ssymbol(p->up);
                if (p->y.p == NULL)
                        p->y.p = genident(STATIC, array(inttype, 0, 0), GLOBAL);
                comment("%s\n", typestring(p->type, p->name));
                defglobal(p->y.p, DATA);
                switch (p->sclass) {
                case ENUM:
                        lc = emit_value(0, inttype, (long)p->u.value);
                        lc = emit_value(lc, voidptype, NULL);
                        break;
                case TYPEDEF:
                        lc = emit_value(0, inttype, 0L);
                        lc = emit_value(lc, voidptype, NULL);
                        break;
                case STATIC: case EXTERN:
                        lc = emit_value(0, inttype, 0L);
                        lc = emit_value(lc, voidptype, p);
                        break;
                default:
                        lc = emit_value(0, inttype, 0L);
                        lc = emit_value(lc, voidptype, p->scope >= PARAM ? NULL : p);
                }
                lc = emit_string(lc, p->name);
                if (p->src.file)
                        lc = emit_string(lc, p->src.file);
                else
                        lc = emit_value(lc, voidptype, NULL);
                lc = emit_value(lc, unsignedchar, (unsigned long)(p->scope > LOCAL ? LOCAL : p->scope));
                lc = emit_value(lc, unsignedchar, (unsigned long)p->sclass);
                lc = emit_value(lc, voidptype, annotate(p->type));
                if (up == NULL)
                        lc = emit_value(lc, voidptype, NULL);
                else if (p->scope == GLOBAL || up->scope != GLOBAL)
                        lc = emit_value(lc, voidptype, up->y.p);
                else
                        lc = emit_value(lc, voidptype, link);
                lc = pad(maxalign, lc);
                nsymbols += lc;

        }
        return p;
}

/* <h3>Execution Points</h3>                                                */
/*                                                                          */
/* The execution-point hook is the easiest one to understand.               */
/* [[point_hook]] is called at each execution point, which occurs just before */
/* every expression and at the beginnings and ends of compound statements.  */
/*                                                                          */
/* <stab functions>=                                                        */
static void point_hook(void *cl, Coordinate *cp, Tree *e) {
        /* <h3>Coordinates</h3>                                                     */
        /*                                                                          */
        /* [[lcc]]'s execution points identify locations at which                   */
        /* breakpoints may be set (see Section 10.2). An execution                  */
        /* point is identified by its source coordinate, which is the               */
        /* triple (file, character position, line number).                          */
        /* When [[glevel]] exceeds 2, [[lcc]] the source                            */
        /* coordinates for every execution point to the list [[loci]],              */
        /* and appends the `tail' of the symbol table corresponding to that execution */
        /* point to [[symbols]]. As execution points are announced, code in         */
        /* [[stab.c]] counts them, so that it can later associate a                 */
        /* index with each execution point.                                         */
        /*                                                                          */
        /* <announce an execution point>=                                           */
        {
                Coordinate *p;
                NEW(p, PERM);
                *p = *cp;
                coordlist = append(p, coordlist);
                epoints++;
        }

        /* [[tail]] points to the [[ssymbol]] structure for the identifier          */
        /* that's the tail of the visible symbols; this symbol provides the context */
        /* for symbol lookups while the control is in the debugger.                 */
        /* By asking the nub to set or clear [[flag]], a debugger can enable and disable */
        /* breakpoints.<p>                                                          */
        /*                                                                          */
        /* Adding the breakpoint code to [[*e]] is accomplished by changing         */
        /* [[*e]] to be a [[RIGHT]] node holding the breakpoint code                */
        /* and the previous value of [[*e]]:                                        */
        /*                                                                          */
        /* <add breakpoint test to *e>=                                             */
        *e = right(tree(AND, voidtype,
                        /* The [[scoordinate]] structure is defined so that [[flag]] is the sign bit. */
                        /* Thus, when [[flag]] is one, the integer in which it appears is negative, and */
                        /* the breakpoint code is really                                            */
                        /* <dl>                                                                     */
                        /* <dd>[[(module.coordinates[i].i < 0 && _Nub_bp(i, tail), *e)]]            */
                        /* </dl>                                                                    */
                        /* The tree for the test is built by calling the constructor for `&lt':     */
                        /*                                                                          */
                        /* <module.coordinates[i].le.flag>=                                         */
                        (*optree['<'])(LT,
                                rvalue((*optree['+'])(ADD,
                                        pointer(idtree(coordinates)),
                                        cnsttree(inttype, (long)epoints))),
                                consttree(0, inttype)),
                        /* [[calltree]] does most of the work in building the IR tree for calling [[_Nub_bp]]: */
                        /*                                                                          */
                        /* <_Nub_bp(i, tail)>=                                                      */
                        calltree(pointer(idtree(nub_bp)), voidtype,
                                tree(ARG+P, voidptype, /* <tail>=                                                                  */
                                                       retype(idtree(tail()), voidptype), tree(
                                     ARG+I, inttype, cnsttree(inttype, (long)epoints), NULL)),
                                NULL)
),
                *e);
}
/* <stab functions>=                                                        */
static Symbol tail(void) {
        Symbol p = allsymbols(identifiers);

        while (p && /* [[emit_symbol]] is like [[annotate]]: it emits an initialized [[ssymbol]] */
                    /* for its argument, and returns the symbol-table entry for that initialized */
                    /* variable. [[emit_symbol]] annotates its argument with the symbol-table   */
                    /* entry for the generated variable, and this annotation marks the symbol   */
                    /* as having been emitted:                                                  */
                    /*                                                                          */
                    /*                                                                          */
                    /* <p is external>=                                                         */
                      (!p->defined
                    && (p->sclass == EXTERN || isfunc(p->type) && p->sclass == AUTO))
)
                p = p->up;
        if (p) {
                /* <append p to symbollist>=                                                */
                if (p->y.p == NULL) {
                        p->y.p = genident(STATIC, array(inttype, 0, 0), GLOBAL);
                        symbollist = append(p, symbollist);
                }

                return p->y.p;
        } else
                return link;
}
/* <stab functions>=                                                        */
static void setoffset(Symbol p, void *tos) {
        /* <append p to symbollist>=                                                */
        if (p->y.p == NULL) {
                p->y.p = genident(STATIC, array(inttype, 0, 0), GLOBAL);
                symbollist = append(p, symbollist);
        }

        walk(asgntree(ASGN,
                rvalue(pointer(idtree(p->y.p))),
                (*optree['-'])(SUB,
                        cast(/* <runtime address of p>=                                                  */
                             isarray(p->type) ? pointer(idtree(p)) : lvalue(idtree(p))
, ptr(chartype)),
                        cast(lvalue(idtree(tos)), ptr(chartype)))),
                0, 0);
        p->addressed = 1;
}

/* <stab functions>=                                                        */
static void entry_hook(void *cl, Symbol cfunc) {
        static int nfuncs;
        Type ty;

        /* Simulates the declaration of an [[sframe]] structure,                    */
        /* but without the tag.                                                     */
        /*                                                                          */
        /* <ty := type for a struct sframe>=                                        */
        ty = newstruct(STRUCT, "");
        #define addfield(name,t) \
                ty->size = roundup(ty->size, t->align);\
                if (ty->align < t->align) ty->align = t->align; \
                newfield(string(name), ty, t)->offset = ty->size; \
                ty->size += t->size
        addfield("up",    voidptype);
        addfield("down",  voidptype);
        addfield("func",  voidptype);
        addfield("module",voidptype);
        addfield("tail",  voidptype);
        addfield("ip",    inttype);     
        #undef addfield
        ty->size = roundup(ty->size, ty->align);
        ty->u.sym->defined = 1;
        ty->u.sym->generated = 1;

        tos = genident(AUTO, ty, LOCAL);
        addlocal(tos);
        tos->defined = 1;
#define set(name,e) walk(asgntree(ASGN,field(lvalue(idtree(tos)),string(#name)),(e)),0,0)
        set(down,       idtree(nub_tos));
        set(func,       idtree(mkstr(cfunc->name)->u.c.loc));
        set(module,     idtree(module));
        set(tail,       cnsttree(voidptype, (void*)0));
#undef set
        walk(asgn(nub_tos, lvalue(idtree(tos))), 0, 0);
        foreach(identifiers, PARAM, setoffset, tos);
}

/* <stab functions>=                                                        */
static void block_hook(void *cl, Symbol *p) {
        while (*p)
                setoffset(*p++, tos);
}

static void exit_hook(void *cl, Symbol cfunc) {
        tos = NULL;
}

/* <stab functions>=                                                        */
static void return_hook(void *cl, Symbol cfunc, Tree *e) {
        walk(asgn(nub_tos, field(lvalue(idtree(tos)), string("down"))), 0, 0);
}

/* If [[e]] is a call, [[stab.c]] changes the expression to                 */
/* the equivalent of the C expression                                       */
/* <dl>                                                                     */
/* <dd>[[(tos.ip = i, tos.tail = tail, *e)]]                                */
/* </dl>                                                                    */
/* where [[i]] is the index in [[coordinates]] for the execution point of   */
/* the expression in which the call appears, and [[tail]] is the corresponding */
/* tail of the list of visible symbols.                                     */
/*                                                                          */
/* <stab functions>=                                                        */
static void call_hook(void *cl, Coordinate *cp, Tree *e) {
        *e = right(
                asgntree(ASGN,
                        field(lvalue(idtree(tos)), string("tail")),
                        /* <tail>=                                                                  */
                        retype(idtree(tail()), voidptype)),
                *e);
        *e = right(
                asgntree(ASGN,
                        field(lvalue(idtree(tos)), string("ip")),
                        consttree(epoints, inttype)),
                *e);
}
/* [[cdb]] symbol tables are emitted differently;                           */
/* the information about symbols is collected                               */
/* as compilation progresses, and the entire symbol table                   */
/* is emitted at the end of compilation.                                    */
/* The only true `stab' routines [[stabinit]] and [[stabend]],              */
/* and [[stab.c]]'s initialization function, [[zstabinit]],                 */
/* modifies the interface record accordingly when the [[-g4]] option has been */
/* specified:                                                               */
/*                                                                          */
/* <stab.c>=                                                                */
static void stabinit(char *file, int argc, char *argv[]) {
        /* <initialization>=                                                        */
        {
                extern Interface sparcIR, solarisIR;
                extern Interface x86IR;
                if (IR == &solarisIR || IR == &sparcIR)
                        leader = "!";
                else if (IR == &x86IR)
                        leader = ";";
                else
                        leader = " #";  /* it's a MIPS or ALPHA */
        }
        /* <initialization>=                                                        */
        {
                extern int getpid(void);
                module = mksymbol(AUTO,
                        stringf("_module__V%x%x", time(NULL), getpid()),
                        array(unsignedtype, 0, 0));
                module->generated = 1;
        }
        /* <initialization>=                                                        */
        coordinates = genident(STATIC, array(inttype, 1, 0), GLOBAL);
        /* <initialization>=                                                        */
        link = genident(STATIC, array(unsignedtype, 0, 0), GLOBAL);

        /* <h2><a name="code">Emitting Debugging Code</a></h2>                      */
        /*                                                                          */
        /* [[stab.c]] arranges for code to be emitted that helps implement the nub  */
        /* interface described below. This code is emitted by building intermediate */
        /* represetation (IR) trees for the desired code, stitching them into       */
        /* the IR trees for the source program, and letting the back end generate   */
        /* the appropriate target machine code. Thus, the debugging code is entirely */
        /* target independent, and it can be described by C code that's equivalent  */
        /* to the IR trees [[stab.c]] builds.<p>                                    */
        /*                                                                          */
        /* [[stab.c]] uses [[lcc]]'s                                                */
        /* `<a href="http://www.cs.princeton.edu/software/lcc/doc/event.html">event hook</a>' */
        /* mechanism to stitch IR trees into the IR tree for the source program     */
        /* (see also Section 8.5). [[stabinit]] arranges for functions to be        */
        /* called before and after the bodies of functions are compiled,            */
        /* when return statements are compiled, when execution points are defined,  */
        /* and when calls are compiled.                                             */
        /*                                                                          */
        /* <initialization>=                                                        */
        attach((Apply) entry_hook, NULL, &events.entry);
        attach((Apply) block_hook, NULL, &events.blockentry);
        attach((Apply) point_hook, NULL, &events.points);
        attach((Apply)  call_hook, NULL, &events.calls);
        attach((Apply)return_hook, NULL, &events.returns);
        attach((Apply)  exit_hook, NULL, &events.exit);
        /* <initialization>=                                                        */
        nub_bp = mksymbol(EXTERN, "_Nub_bp", ftype(voidtype, inttype));
        nub_bp->defined = 0;
        /* [[allsymbols]] returns the symbol at the head                            */
        /* of the list of symbol-table entries that represent                       */
        /* the tails of the visible symbols. This list is threaded through the      */
        /* [[up]] fields in symbols.                                                */
        /*                                                                          */
        /*                                                                          */
        /* <initialization>=                                                        */
        nub_tos = mksymbol(EXTERN, "_Nub_tos", voidptype);
        nub_tos->defined = 0;
}

void zstab_init(int argc, char *argv[]) {
        int i;
        static int inited;

        if (inited)
                return;
        assert(IR);
        inited = 1;
        for (i = 1; i < argc; i++)
                if (strcmp(argv[i], "-g4") == 0) {
                        IR->stabinit  = stabinit;
                        IR->stabend   = stabend;
                        IR->stabblock = 0;
                        IR->stabfend  = 0;
                        IR->stabline  = 0;
                        IR->stabsym   = 0;
                        IR->stabtype  = 0;
                        glevel = 4;
                        break;
                }
}
