#ifndef GLUE_INCLUDED
#define GLUE_INCLUDED

/* $Id$ */
struct module {
        union scoordinate *coordinates;
        char **files;
        struct ssymbol *link;
};

struct stype {
        unsigned short len;
        unsigned char op;
        unsigned size;
        union {
                struct {	/* pointers */
                        struct stype *type;
                } p;
                struct {	/* arrays */
                        struct stype *type;
                        unsigned nelems;
                } a;
                struct {	/* structs/unions */
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
                struct {	/* enums */
                        char *tag;
                        struct {
                                char *name;
                                int value;
                        } enums[1];
                } e;
                struct {	/* functions */
                        struct stype *type;
                        struct stype *args[1];
                } f;
                struct {	/* qualified types */
                        struct stype *type;
                } q;

        } u;
};

union scoordinate {
        int i;
        struct { unsigned int y:16,x:10,index:5,flag:1; } le;
        struct { unsigned int flag:1,index:5,x:10,y:16; } be;
};

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

struct sframe {
        struct sframe *up, *down;
        char *func;
        struct module *module;
        struct ssymbol *tail;
        int ip;
};
#endif
