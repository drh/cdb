#ifndef GLUE_INCLUDED
#define GLUE_INCLUDED

/* $Id$ */

typedef unsigned short uint16;

struct module {
        union scoordinate *coordinates;
        char **files;
        struct ssymbol *link;
	uint16 strcount;
	const char *strings;
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
                        uint16 tag;
                        struct {
                                uint16 name;
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
                        uint16 tag;
                        struct {
                                uint16 name;
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
        uint16 name;
        uint16 file;
        unsigned char scope;
        unsigned char sclass;
	struct module *module;
        struct stype *type;
        struct ssymbol *uplink;
};

struct sframe {
        struct sframe *up, *down;
        const char *func;
        struct module *module;
        struct ssymbol *tail;
        int ip;
};
#endif
