# $Id$
INCLUDES=-I//atr/users/drh/lib/cii/1/include -I/users/drh/book/4.0/src
NOTANGLE=notangle -L -t8
CC=cc
SRCDIR=c:/users/drh/book/4.0
LD=cc
LDFLAGS=-g -L//atr/users/drh/lib/cii/1 -L/users/drh/book/4.0/x86/win32
E=.exe
O=.obj
A=.lib

all::

clobber::
		rm -f $(BUILDDIR)/prelink.sh
		rm -f $(BUILDDIR)/*.ilk $(BUILDDIR)/*.pdb
