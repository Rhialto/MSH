# $Id: Makefile,v 1.6 90/03/11 17:48:07 Rhialto Rel $
#
# Makefile for messydos file system handler and -device, for use with the
# Manx C compiler version 3.6.
#
# This code is (C) Copyright 1989,1990 by Olaf Seibert. All rights reserved.
# May not be used or copied without a licence.

HOBJ =	pack.o support.o hanmain.o hansec.o hanlock.o hanfile.o hanreq.o \
	hancmd.o date.o
HSRC =	pack.c support.c hanmain.c hansec.c hanlock.c hanfile.c hanreq.c \
	hancmd.c date.c dos.h han.h
DOBJ =	device.o devio.o
DSRC =	device.c devio.c dev.h device.h
XSRC =	messyfmt.c Makefile ignore.c die.c
#DB =	 -DDEBUG -DHDEBUG
#DB =	 -DHDEBUG
#WACK =  -W

#   Options: large code, large data, no startup code, use A4
.c.o:
	cc +Iamiga.syms +cdbrx3,5 $(DB) $*.c -o $@

all:	MessyFileSystem messydisk.device
utils:	die ignore messyfmt
doc:	dev.man msh.man

messydisk.device: $(DOBJ)
	ln -o $@ $(WACK) $(DOBJ) -lcl

MessyFileSystem:    $(HOBJ)
	ln -o $@ $(WACK) $(HOBJ) -lcl

Die:	die.c
	cc +Iamiga.syms +x3,5 die.c
	ln Die.o -lcl

Ignore: ignore.c dev.h device.h
	cc +Iamiga.syms +x3,5 ignore.c
	ln Ignore.o -lcl

MessyFmt:
	cc +Iamiga.syms +x3,5 messyfmt.c
	ln MessyFmt.o -lcl

dev.man: dev.n
	nro dev.n >dev.man

msh.man: msh.n
	nro msh.n >msh.man

ci:
	RCS:ci -u $(HSRC) $(DSRC) $(XSRC)

co:
	RCS:co -l $(HSRC) $(DSRC) $(XSRC)

# DO NOT DELETE THIS LINE - important for making dependencies!
#|.c|$*.o|
#|.h|
# From this line on, everything has been created by MakeDepend.
# Anything you add yourself will be deleted automagically.

date.o: dev.h han.h date.c
devio.o: device.h dev.h devio.c
hanfile.o: dev.h han.h dos.h hanfile.c
support.o: dos.h support.c
device.o: device.h dev.h device.c
hanlock.o: dev.h han.h dos.h hanlock.c
hanmain.o: dev.h han.h dos.h hanmain.c
hansec.o: dev.h han.h dos.h hansec.c
hanreq.o: dos.h hanreq.c
messyfmt.o: dev.h han.h messyfmt.c
pack.o: dev.h han.h dos.h pack.c
hancmd.o: dev.h han.h hancmd.c
