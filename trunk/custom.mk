# $Id$
INCLUDES=-I//atr/users/drh/lib/cii/1/include -I/users/drh/book/4.0/src -I//atr/users/drh/include
NOTANGLE=notangle -L -t8
CC=cc
SRCDIR=c:/users/drh/book/4.0
LD=cc
LDFLAGS=-g -L//atr/users/drh/lib/cii/1 -L/users/drh/book/4.0/x86/win32 -L//atr/users/drh/lib
LIBS=-lcii ws2_32.lib
E=.exe
O=.obj
A=.lib

all::		$(BUILDDIR)/startup$O src/inits.c $(BUILDDIR)/liblcc$A $(BUILDDIR)/cpp$E

$(BUILDDIR)/startup$O:	;	cp startup/`basename $(BUILDDIR)`$O $@
$(BUILDDIR)/liblcc$A:	;	cp $(SRCDIR)/$@ $@
$(BUILDDIR)/cpp$E:	;	cp $(SRCDIR)/$@ $@

clobber::
		rm -f $(BUILDDIR)/prelink.sh src/inits.c $(BUILDDIR)/cpp$E $(BUILDDIR)/liblcc$A
		rm -f $(BUILDDIR)/*.ilk $(BUILDDIR)/*.pdb $(BUILDDIR)/*.idb $(BUILDDIR)/*.pch
		rm -f *.exe *$O *.asm *.s *.map *.pdb *.ilk *.idb

src/inits.c:	$(SRCDIR)/src/inits.c makefile
		sed '/^}/d' <$(SRCDIR)/src/inits.c >$@
		echo '\t{extern void zstab_init(int, char *[]); zstab_init(argc, argv);}' >>$@
		echo '}\nextern int main(int, char *[]);' >>$@

