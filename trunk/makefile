CFLAGS	= -g
INCLUDES= -I/usr/local/lib/cii/1/include -I/u/drh/book/4.0/src
LIBS	= -lcii
LDFLAGS	= -g
BUILDDIR=x86/win32
E=
O=.o
A=.a
CUSTOM=custom.mk
include $(CUSTOM)

all::		$(BUILDDIR)/libnub$A $(BUILDDIR)/cdb$E $(BUILDDIR)/stab$O
 
$(BUILDDIR)/libnub$A:	$(BUILDDIR)/server$O $(BUILDDIR)/nub$O $(BUILDDIR)/symstub$O
			ar ruv $@ $?

CDBOBJS=$(BUILDDIR)/client$O \
	$(BUILDDIR)/cdb$O \
	$(BUILDDIR)/symtab$O

$(BUILDDIR)/cdb$E:	$(CDBOBJS)
			$(CC) $(LDFLAGS) -o $@ $(CDBOBJS) $(LIBS)

$(BUILDDIR)/cdb$O:	src/cdb.c src/server.h src/glue.h src/nub.h src/symtab.h
			$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/cdb.c

$(BUILDDIR)/nub$O:	src/nub.c src/nub.h src/glue.h
			$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/nub.c

$(BUILDDIR)/symstub$O:	src/symstub.c src/symtab.h
			$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/symstub.c

$(BUILDDIR)/symtab$O:	src/symtab.c src/symtab.h src/glue.h
			$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/symtab.c

$(BUILDDIR)/server$O:	src/server.c src/server.h src/nub.h src/glue.h
			$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/server.c

$(BUILDDIR)/client$O:	src/client.c src/server.h src/nub.h src/glue.h
			$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/client.c

$(BUILDDIR)/stab$O:	src/stab.c src/glue.h
			$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/stab.c


clean::
		rm $(BUILDDIR)/*$O

clobber::	clean
		rm -f $(BUILDDIR)/libnub$A $(BUILDDIR)/cdb$E
