#ifndef GLUE_INCLUDED
#define GLUE_INCLUDED

/* $Id$ */

struct module {
	union scoordinate *coordinates;
	struct ssymbol **tails;
        char **files;
	struct ssymbol *globals;
	int length;
	const char *constants;
};

struct stype {
        unsigned short len;
        unsigned char op;
        unsigned size;
        union {
                struct {	/* pointers */
                        const struct stype *type;
                } p;
                struct {	/* arrays */
                        const struct stype *type;
                        unsigned nelems;
                } a;
                struct {	/* structs/unions */
                        const char *tag;
                        struct {
                                const char *name;
                                const struct stype *type;
                                union offset {
                                        unsigned off;
                                        int offset;
                                        struct { int size:6; unsigned lsb:6, offset:8*sizeof(int)-12; } be;
                                        struct { unsigned offset:8*sizeof(int)-12, lsb:6; int size:6; } le;
                                } u;
                        } fields[1];
                } s;
                struct {	/* enums */
                        const char *tag;
                        struct {
                                const char *name;
                                int value;
                        } enums[1];
                } e;
                struct {	/* functions */
                        const struct stype *type;
                        const struct stype *args[1];
                } f;
                struct {	/* qualified types */
                        const struct stype *type;
		} q;
        } u;
};

union scoordinate {
        int i;
        struct { unsigned int y:16,x:10,index:5,flag:1; } le;
        struct { unsigned int flag:1,index:5,x:10,y:16; } be;
};

struct ssymbol {
	union {
		int value;
		int offset;
		void *address;
	} u;
	const char *name;
	const char *file;
	unsigned char scope;
	unsigned char sclass;
	struct module *module;
        const struct stype *type;
	const struct ssymbol *uplink;
};

struct sframe {
        struct sframe *up, *down;
        const char *func;
        struct module *module;
        int ip;
};

#endif
