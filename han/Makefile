# $Id: Makefile,v 1.46 91/10/06 18:27:07 Rhialto Rel $
#
# Makefile for messydos file system handler, for use with the
# DICE C compiler.
#
# This code is (C) Copyright 1989-1992 by Olaf Seibert. All rights reserved.
# May not be used or copied without a licence.

#DB =	 -DHDEBUG
#DBOBJ = syslog.o
#DB =	 -DDEBUG
#WACK =  -s
REGARGS = -mRR
PURE = -r -mS
CLIB =	csr.lib  # when regargs are used set this to cr.lib
#CLIB = cs.lib	 # when no regargs are used set this to c.lib

HOBJ =	hancrtso.o pack.o support.o hanmain.o hansec.o hanlock.o hanfile.o \
	hanconv.o hanreq.o hancmd.o date.o $(DBOBJ)
HSRC =	hancrtso.a pack.c support.c hanmain.c hansec.c hanlock.c hanfile.c \
	hanconv.c hanreq.c hancmd.c date.c dos.h han.h
XSRC =	messyfmt.c Makefile ignore.c die.c
DSRC =	device1.a device2.c devio1.a devio2.c dev.h device.h

.SUFFIXES: .a

#   Options:
.c.o:
	dcc -c -proto $(REGARGS) $(PURE) $(DB) $*.c -o $@

.a.o:
	a68k -q $*.a -o$@

all:	MessyFileSystem messydisk.device
utils:	die ignore messyfmt
doc:	dev.man msh.man

messydisk.device: $(DSRC)
	make -f makefile.dev

MessyFileSystem:    hanproto.h $(HOBJ)
	dcc -o $@ -l0 $(REGARGS) $(PURE) $(WACK) $(HOBJ) $(CLIB) amigas13.lib

hancrtso.o: hancrtso.a
	dcc -c -o hancrtso.o hancrtso.a

Die:	die.c
	dcc -proto $(PURE) die.c -o Die

Ignore: ignore.c dev.h device.h
	dcc -proto $(PURE) ignore.c -o Ignore

MessyFmt:   MessyFmt.c han.h
	dcc -proto $(PURE) messyfmt.c -o MessyFmt

hanproto.h: $(HSRC)
	makeproto -o hanproto.h $(HSRC)

dev.man: dev.n
	nro dev.n >dev.man

msh.man: msh.n
	nro msh.n >msh.man

ci:
	RCS:ci -u $(HSRC) $(XSRC)

co:
	RCS:co -l $(HSRC) $(XSRC)

# DO NOT DELETE THIS LINE - important for making dependencies!
#|.c|$*.o|
#|.h|
# From this line on, everything has been created by MakeDepend.
# Anything you add yourself will be deleted automagically.

date.o: dev.h han.h date.c
#mkci.o: device.h dev.h mkci.c
hanfile.o: dos.h dev.h han.h hanconv.h hanfile.c
support.o: dos.h support.c
hanmain.o: dos.h dev.h han.h hanconv.h hanmain.c
hanlock.o: dos.h dev.h han.h hanlock.c
hansec.o: dos.h dev.h han.h hansec.c
hanreq.o: dos.h dev.h han.h hanreq.c
pack.o: dos.h dev.h han.h pack.c
hancmd.o: dev.h han.h hancmd.c
hanconv.o: dev.h han.h hanconv.h hanconv.c
