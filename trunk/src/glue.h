#ifndef GLUE_INCLUDED
#define GLUE_INCLUDED

/* The name [[zstab_init]] is chosen so that it will be the last            */
/* initialization function called by [[init]].<p>                           */
/*                                                                          */
/* Symbol tables are just initialized data structures, and they             */
/* are thus machine independent. The following EBNF grammar defines         */
/* the gross structure of these tables. Except for [[module]],              */
/* defined nonterminals denote                                              */
/* pointers structures or arrays, and undefined nonterminals denote         */
/* structures, which are described below. As usual, { <i>X</i> } means      */
/* zero or more occurrences of <i>X</i>, but it also denotes an array       */
/* of <i>X</i>. [[NULL]] is the null pointer, so                            */
/* { <i>X</i> } [[NULL]] denotes a null-terminated array of <i>X</i>.<p>    */
/*                                                                          */
/* <tt>                                                                     */
/* <dl>                                                                     */
/* <dt>module:                                                              */
/*         <dd>coordinates files link                                       */
/* <dt>coordinates:                                                         */
/*         <dd>{ scoordinate }                                              */
/* <dt>files:                                                               */
/*         <dd>{ </tt>a null-terminated ASCII string<tt> } NULL             */
/* <dt>link:                                                                */
/*         <dd>ssymbol                                                      */
/* </dl>                                                                    */
/* </tt>                                                                    */
/* The definitions of each of the structures suggested by this grammar      */
/* define the detailed structure of the symbol table,                       */
/* and it is these structures that the functions in [[stab.c]] must emit.   */
/* Names like [[scoordinate]] and                                           */
/* [[ssymbol]] are used to avoid conflicts with the                         */
/* similarly named structures in [[lcc]].<p>                                */
/*                                                                          */
/* A [[module]] is a structure containing 4 pointers:                       */
/*                                                                          */
/* <glue.h types>=                                                          */
struct module {
        union scoordinate *coordinates;
        char **files;
        struct ssymbol *link;
};
/* [[types]] points to an array of [[stype]] structures.                    */
/* Similarly,                                                               */
/* [[coordinates]] points to an array of [[scoordinate]] structures,        */
/* [[files]] points to a null-terminated array of strings,                  */
/* and [[link]] points to a [[ssymbol]] structure.<p>                       */
/*                                                                          */
/* <h3>Types</h3>                                                           */
/*                                                                          */
/* Symbol-table entries, [[ssymbols]], point to types, which are encoded in */
/* [[stype]] structures:                                                    */
/*                                                                          */
/* <glue.h types>=                                                          */
struct stype {
        unsigned short len;
        unsigned char op;
        unsigned size;
        union {
                /* For a pointer, [[u.p.type]] points to the [[stype]] for the referent:    */
                /*                                                                          */
                /* <pointers>=                                                              */
                struct {
                        struct stype *type;
                } p;

                /* For an array, [[u.a]] points to the type of the elements and holds       */
                /* the number of elements:                                                  */
                /*                                                                          */
                /* <arrays>=                                                                */
                struct {
                        struct stype *type;
                        unsigned nelems;
                } a;

                /* For a structure or a union, [[u.s]] points to the tag and holds          */
                /* an array of triples that give each field's name, type, and offset        */
                /* in bytes:                                                                */
                /*                                                                          */
                /* <aggregates>=                                                            */
                struct {
                        char *tag;
                        struct {
                                char *name;
                                struct stype *type;
                                union offset {
                                        unsigned off;
                                        int offset;
                                        struct { int size:6; unsigned lsb:6, offset:8*sizeof(int)-12; } be;
                                        struct { unsigned offset:8*sizeof(int)-12, lsb:6; int size:6; } le;
                                } u;
                        } fields[1];
                } s;

                /* [[mkstr]] generates a string literal, arranges for it to be emitted      */
                /* later, and returns the symbol-table entry for the literal (see           */
                /* <a href="http://www.cs.princeton.edu/software/lcc/lcc/src/sym.c">[[src/sym.c]]</a>).<p> */
                /*                                                                          */
                /* For enums, the [[u.e]] field points to the tag and holds                 */
                /* a null-terminated array of name-value pairs:                             */
                /*                                                                          */
                /* <enums>=                                                                 */
                struct {
                        char *tag;
                        struct {
                                char *name;
                                int value;
                        } enums[1];
                } e;

                /* For a function type, [[u.f]] points to the return type                   */
                /* and holds a null-terminated array of types for each of the arguments:    */
                /*                                                                          */
                /* <functions>=                                                             */
                struct {
                        struct stype *type;
                        struct stype *args[1];
                } f;

                /* <qualified types>=                                                       */
                struct {
                        struct stype *type;
                } q;

        } u;
};
/* [[stabend]] emits a one-word rendition for each execution point:         */
/*                                                                          */
/* <glue.h types>=                                                          */
union scoordinate {
        int i;
        struct { unsigned int y:16,x:10,index:5,flag:1; } le;
        struct { unsigned int flag:1,index:5,x:10,y:16; } be;
};
/* <h3>Symbols</h3>                                                         */
/*                                                                          */
/* The [[ssymbol]] structures emitted by [[stab.c]] carry all the information */
/* about an identifier needed by a debugger. [[ssymbol]]s are compact       */
/* renditions of [[Symbol]]s:                                               */
/*                                                                          */
/* <glue.h types>=                                                          */
struct ssymbol {
        int offset;
        void *address;
        char *name;
        char *file;
        unsigned char scope;
        unsigned char sclass;
        struct stype *type;
        struct ssymbol *uplink;
};
/* <glue.h types>=                                                          */
struct sframe {
        struct sframe *up, *down;
        char *func;
        struct module *module;
        struct ssymbol *tail;
        int ip;
};
#endif
