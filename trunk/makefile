# $Id$
CC=lcc
CFLAGS	= -g
# Configuration:
# SRCDIR	root of the lcc 4.1 distribution
# INCLUDES	-I options for CII headers and lcc 4.0 headers
# HOSTFILE	path to an lcc driver back end that supports cdb
# BUILDDIR	the lcc 4.1 build directory
# LDFLAGS	-L options for librcc.a (in the lcc build directory)
# LIBS		libraries, including socket and network libraries
SRCDIR	= /u/drh/pkg/lcc/4.1
INCLUDES= -I/usr/local/lib/cii/1/include -I$(SRCDIR)/src -I$(ASDL_HOME)/include/asdlGen -I$(BUILDDIR)
HOSTFILE=etc/solaris.c
BUILDDIR= $(SRCDIR)/sparc-solaris
LDFLAGS	= -g -L$(BUILDDIR)
LIBS	= -lcii -lsocket -lnsl
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
libnub:		$Blibnub$A $Bclientstub$O
cdb:		$Bcdb$E

$Blcc$E:	$Blcc$O $Bhost$O;	$(CC) $(LDFLAGS) -o $@ $Blcc$O $Bhost$O

$Blcc$O:	$(SRCDIR)/etc/lcc.c;	$(CC) -v -c $(CFLAGS) -o $@ $(SRCDIR)/etc/lcc.c
$Bhost$O:	$(HOSTFILE);		$(CC) -c $(CFLAGS) -o $@ $(HOSTFILE)
 
$Blibnub$A:	$Bclient$O $Bnub$O $Bsymstub$O $Bcomm$O
		ar ruv $@ $?

$Bprelink.sh:	src/prelink.sh;		cp src/prelink.sh $@; chmod +x $@

$Brcc$E:	$Bstab$O $Binits$O $Bsym$O
		$(CC) $(LDFLAGS) -o $@ $Bstab$O $Binits$O $Bsym$O -lrcc -lcii -L$(ASDL_HOME)/lib/asdlGen -lasdl

$Binits$O:	src/inits.c;	$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/inits.c

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

$Bstab$O:	src/stab.c src/glue.h $Bsym.h
		$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ src/stab.c

$Bsym.h:	src/sym.asdl
		$(ASDL_HOME)/bin/asdlGen --c -d $B src/sym.asdl

$Bsym$O:	$Bsym.h
		$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $Bsym.c

stubtest:	wf.c lookup.c $Bcdb$O
		$Blcc -Wo-lccdir=$(BUILDDIR) -v -Wo-g4 wf.c lookup.c $Bcdb$O \
			`if [ -d c:/ ]; then echo libcii.lib; else echo -lcii; fi`

test:		wf.c lookup.c $Blibnub$A $Bcdb$E
		$Blcc -Wo-lccdir=$(BUILDDIR) -v -Wo-g4 wf.c lookup.c

clean::
		rm -f $B*$O

clobber::	clean
		rm -f $Blibnub$A $Bcdb$E $Brcc$E $Blcc$E $Bprelink.sh
