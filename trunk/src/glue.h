#ifndef GLUE_INCLUDED
#define GLUE_INCLUDED

/* $Id$ */

typedef unsigned int u4;

struct module {
	u4 uid;
        union scoordinate *coordinates;
        char **files;
	struct ssymbol *link;
	u4 length;
	const char *constants;
};

struct stype {
        unsigned short len;
        unsigned char op;
        unsigned size;
        union {
                struct {	/* pointers */
                        u4 type;
                } p;
                struct {	/* arrays */
                        u4 type;
                        unsigned nelems;
                } a;
                struct {	/* structs/unions */
                        u4 tag;
                        struct {
                                u4 name;
                                u4 type;
                                union offset {
                                        unsigned off;
                                        int offset;
                                        struct { int size:6; unsigned lsb:6, offset:8*sizeof(int)-12; } be;
                                        struct { unsigned offset:8*sizeof(int)-12, lsb:6; int size:6; } le;
                                } u;
                        } fields[1];
                } s;
                struct {	/* enums */
                        u4 tag;
                        struct {
                                u4 name;
                                int value;
                        } enums[1];
                } e;
                struct {	/* functions */
                        u4 type;
                        u4 args[1];
                } f;
                struct {	/* qualified types */
                        u4 type;
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
	u4 self;
        u4 name;
        u4 file;
        unsigned char scope;
        unsigned char sclass;
	struct module *module;
        u4 type;
	u4 uplink;
};

struct sframe {
        struct sframe *up, *down;
        const char *func;
        struct module *module;
        struct ssymbol *tail;
        int ip;
};

#endif
