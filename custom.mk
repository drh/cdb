# $Id$
INCLUDES=-I//atr/users/drh/lib/cii/1/include -I/users/drh/book/4.0/src
NOTANGLE=notangle -L -t8
CC=cc
LDFLAGS=-g -L//atr/users/drh/lib/cii/1
E=.exe
O=.obj
A=.lib

all::		$(BUILDDIR)/cdbld.sh

src/%.c:	cdb.nw;		$(NOTANGLE) -R$(@:f) cdb.nw >$@
src/%.h:	cdb.nw;		$(NOTANGLE) -R$(@:f) cdb.nw >$@

%/cdbld.sh:	cdb.nw
		notangle -t8 -R$@ cdb.nw >$@
		chmod +x $@

clobber::
		rm -f src/*.[ch] $(BUILDDIR)/cdbld.sh
		rm -f $(BUILDDIR)/*.ilk $(BUILDDIR)/*.pdb
