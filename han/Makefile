# $Id: Makefile,v 1.3 89/12/17 20:22:35 Rhialto Exp Locker: Rhialto $
#
#   Makefile for messydos file system handler and -device.
#
#   This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
#   not be used or copied without a licence.

HOBJ =	pack.o support.o hanmain.o hansec.o hanlock.o hanfile.o hanreq.o date.o
HSRC =	pack.c support.c hanmain.c hansec.c hanlock.c hanfile.c hanreq.c date.c \
	dos.h han.h
DOBJ =	device.o devio.o
DSRC =	device.c devio.c dev.h device.h
XSRC =	messyfmt.c Makefile
DB =	-DDEBUG
#WACK =  -W

#   Options: large code, large data, no startup code, use A4
.c.o:
	cc +Iamiga.syms +cdbrx3,5 $(DB) $*.c -o $@

all:	msh messydisk.device
utils:	die ignore messyfmt
doc:	dev.man msh.man

messydisk.device: $(DOBJ)
	ln -o $@ $(WACK) $(DOBJ) -lcl

msh:	$(HOBJ)
	ln -o $@ $(WACK) $(HOBJ) -lcl

die:	die.c
	cc +Iamiga.syms +x3,5 die.c
	ln die.o -lcl

ignore: ignore.c dev.h device.h
	cc +Iamiga.syms +x3,5 ignore.c
	ln ignore.o -lcl

messyfmt: messyfmt.c dev.h device.h han.h
	cc +Iamiga.syms +x3,5 messyfmt.c
	ln messyfmt.o -lcl

dev.man: dev.n
	nro dev.n >dev.man

msh.man: msh.n
	nro msh.n >msh.man

ci:
	RCS:ci -u $(HSRC) $(DSRC) $(XSRC)

co:
	RCS:co -l $(HSRC) $(DSRC) $(XSRC)

device.o: dev.h device.h
devio.o:  dev.h device.h
hanmain.o:  han.h dev.h
hansec.o:  han.h dev.h
hanlock.o:  dos.h han.h dev.h
hanfile.o:  dos.h han.h dev.h
support.o:  dos.h
pack.o: dos.h han.h dev.h
