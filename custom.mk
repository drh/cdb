# $Id$
SRCDIR=/users/drh/book/4.2
INCLUDES=-I/users/drh/lib/cii/1/include -I$(SRCDIR)/src -I$(BUILDDIR) -I$(ASDL_HOME)/include/asdlGen -I/users/drh/include
NOTANGLE=notangle -L -t8
CC=cc
CFLAGS=-g -DWIN32
LD=cc
HOSTFILE=etc/win32.c
BUILDDIR=x86/win32
LDFLAGS=-g
LIBS=-Wl$(ASDL_HOME)/lib/asdlGen/libasdl.lib -lcii -lws2_32.lib
lib:=/users/drh/lib/cii/1;$(SRCDIR)/x86/win32;/Program Files/Microsoft Visual Studio/vc98/lib
LCCINPUTS=.;\users\drh\lib\cii\1
.EXPORT:	lib LCCINPUTS
E=.exe
O=.obj
A=.lib

all::		$(BUILDDIR)/startup$O src/inits.c $(BUILDDIR)/liblcc$A $(BUILDDIR)/cpp$E $(BUILDDIR)/sym.typ

$(BUILDDIR)/startup$O:	;	cp startup/`basename $(BUILDDIR)`$O $@
$(BUILDDIR)/liblcc$A:	;	cp $(SRCDIR)/$@ $@
$(BUILDDIR)/cpp$E:	;	cp $(SRCDIR)/$@ $@

clobber::
		rm -f $(BUILDDIR)/prelink.sh src/inits.c $(BUILDDIR)/cpp$E $(BUILDDIR)/liblcc$A
		rm -f $(BUILDDIR)/*.ilk $(BUILDDIR)/*.pdb $(BUILDDIR)/*.idb $(BUILDDIR)/*.pch
		rm -f *.exe *$O *.asm *.s *.map *.pdb *.ilk *.idb
		rm -f packing.lst

$(BUILDDIR)/sym.typ:	src/sym.asdl
			$(ASDL_HOME)/bin/asdlGen --typ -o $@ $^

src/inits.c:	$(SRCDIR)/src/inits.c makefile
		sed -e '/^}/d' -e '/asdl/d' <$(SRCDIR)/src/inits.c >$@
		echo '\t{extern void zstab_init(int, char *[]); zstab_init(argc, argv);}' >>$@
		echo '}\nextern int main(int, char *[]);' >>$@

packing.lst:	src/inits.c custom.mk
		ls src/*.asdl src/*.[ch] src/*.sh etc/*.c startup/*.o* README makefile lookup.[ch] wf.c >$@
		echo $@ >>$@
