# $Id$
CFLAGS	= -g
INCLUDES= -I/usr/local/lib/cii/1/include -I/u/drh/pkg/lcc/4.0
LIBS	= -lcii
LDFLAGS	= -g -L/u/drh/pkg/lcc/4.0/sparc-solaris
HOSTFILE=etc/win32.c
BUILDDIR=x86/win32
E=
O=.o
A=.a
CUSTOM=custom.mk
include $(CUSTOM)

B=$(BUILDDIR)/

all::		libnub cdb rcc lcc prelink

prelink:	$Bprelink.sh
lcc:		$Blcc$E
rcc:		$Brcc$E
libnub:		$Blibnub$A
cdb:		$Bcdb$E

$Blcc$E:	$Blcc$O $Bhost$O;	$(LD) $(LDFLAGS) -o $@ $Blcc$O $Bhost$O

$lcc$O:		$(SRCDIR)/etc/lcc.c;	$(CC) -v -c $(CFLAGS) -o $@ $(SRCDIR)/etc/lcc.c
$Bhost$O:	$(HOSTFILE);		$(CC) -c $(CFLAGS) -o $@ $(HOSTFILE)
 
$Blibnub$A:	$Bclient$O $Bnub$O $Bsymstub$O $Bcomm$O
		ar ruv $@ $?

$Bprelink.sh:	src/prelink.sh;		cp src/prelink.sh $@

$Brcc$E:	$Bstab$O $Binits$O
		$(LD) $(LDFLAGS) -o $@ $Bstab$O $Binits$O -lrcc -lcii

$Binits$O:	src/inits.c;	$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/inits.c

src/inits.c:	$(SRCDIR)/src/inits.c makefile
		sed '/^}/d' <$(SRCDIR)/src/inits.c >$@
		echo '\t{extern void zstab_init(int, char *[]); zstab_init(argc, argv);}' >>$@
		echo '}\nextern int main(int, char *[]);' >>$@

CDBOBJS=$Bserver$O \
	$Bcomm$O \
	$Bcdb$O \
	$Bsymtab$O

$Bcdb$E:	$(CDBOBJS)
		$(CC) $(LDFLAGS) -o $@ $(CDBOBJS) $(LIBS)

$Bcdb$O:	src/cdb.c src/server.h src/glue.h src/nub.h src/symtab.h
		$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/cdb.c

$Bcomm$O:	src/comm.c src/comm.h src/server.h
		$(CC) -c $(CFLAGS) -o $@ src/comm.c

$Bserver$O:	src/server.c src/comm.h src/glue.h src/nub.h src/server.h
		$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/server.c

$Bclient$O:	src/client.c src/comm.h src/glue.h src/nub.h src/server.h
		$(CC) -c $(CFLAGS) -o $@ src/client.c

$Bclientstub$O:	src/clientstub.c src/glue.h src/nub.h
		$(CC) -c $(CFLAGS) -I$(SRCDIR)/src -o $@ src/clientstub.c

$Bnub$O:	src/nub.c src/nub.h src/glue.h
		$(CC) -c $(CFLAGS) -I$(SRCDIR)/src -o $@ src/nub.c

$Bsymstub$O:	src/symstub.c src/symtab.h
		$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/symstub.c

$Bsymtab$O:	src/symtab.c src/symtab.h src/glue.h
		$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/symtab.c

$Bstab$O:	src/stab.c src/glue.h
		$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/stab.c

test:		wf.c lookup.c $Blibnub$A $Bcdb$E
		$Blcc -Wo-lccdir=$(BUILDDIR) -v -Wo-g4 wf.c lookup.c

clean::
		rm -f $B*$O

clobber::	clean
		rm -f $Blibnub$A $Bcdb$E $Brcc$E $Bprelink.sh
