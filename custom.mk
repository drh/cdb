# $Id$
INCLUDES=-I/users/drh/lib/cii/1/include -I/users/drh/book/4.0/src -I/users/drh/include
NOTANGLE=notangle -L -t8
CC=cc
CFLAGS=-g -DWIN32
LD=cc
SRCDIR=c:/users/drh/book/4.0
HOSTFILE=etc/win32.c
BUILDDIR=x86/win32
LDFLAGS=-g
LIBS	= -lcii ws2_32.lib
LIB=/users/drh/lib/cii/1;/users/drh/book/4.0/x86/win32;/Program Files/devstudio/vc/lib;e:/pkg/devstudio/vc/lib
LCCINPUTS=.;\users\drh\lib\cii\1
.EXPORT:	LIB LCCINPUTS
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
		rm -f packing.lst

src/inits.c:	$(SRCDIR)/src/inits.c makefile
		sed '/^}/d' <$(SRCDIR)/src/inits.c >$@
		echo '\t{extern void zstab_init(int, char *[]); zstab_init(argc, argv);}' >>$@
		echo '}\nextern int main(int, char *[]);' >>$@

packing.lst:	src/inits.c custom.mk
		ls src/*.[ch] src/*.sh etc/*.c startup/*.o* README makefile lookup.[ch] wf.c >$@
		echo $@ >>$@
