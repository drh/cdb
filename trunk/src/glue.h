#ifndef GLUE_INCLUDED
#define GLUE_INCLUDED

/* $Id$ */

struct module {
	unsigned int uname;
	void **addresses;
};

struct sframe {
	struct sframe *up, *down;
	int func;
	int module;
	int ip;
};

#endif
