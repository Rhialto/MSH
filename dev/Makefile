# $Id: Makefile,v 1.58 2005/10/19 16:51:06 Rhialto Exp $
#
# Makefile for messydos device, for use with the DICE C compiler.
#
# This code is (C) Copyright 1989-1997 by Olaf Seibert. All rights reserved.
# May not be used or copied without a licence.

# DBOBJ = /syslog/syslogi.o
# DB =	  -d2 -DDEBUG=1 -I/syslog
# WACK =  -d2 -s

REGARGS = -mRR -mi
PURE = -mS
HDR =	-Hdevice.sym=device.h

DOBJ =	device1.o device2.o devio1.o devio2.o $(DBOBJ)
DSRC =	device1.a device2.c devio1.a devio2.c dev.h device.h layout.h
XSRC =	mkci.c Makefile /util/ignore.c
RCSVER = -r1.58

.SUFFIXES: .a

#   Options:
.c.o:
	dcc -c -proto $(REGARGS) $(PURE) $(DB) $(HDR) $*.c -o $@

.a.o:
	a68k -q $*.a -o$@

all:	/messydisk.device /c/Ignore

/messydisk.device: devproto.h $(DOBJ)
	dcc -o $@ -l0 $(REGARGS) $(WACK) $(DOBJ) -lc -lamiga30

Mkci:	mkci.c dev.h device.h
	dcc mkci.c $(HDR) -o Mkci

/c/Ignore: /util/ignore.c dev.h device.h
	dcc -proto $(PURE) $(HDR) /util/ignore.c -o /c/Ignore

device1.o: device1.a c.i
devio1.o: devio1.a c.i

devproto.h: $(DSRC)
	-delete devproto.h device.sym
	makeproto -o devproto.h $(DSRC)

c.i:	Mkci
	Mkci c.i

ci:
	RCS:ci -f -u $(RCSVER) $(DSRC) $(XSRC)

co:
	RCS:co -u $(DSRC) $(XSRC)

depend:
	mkd -I/syslog/ -f $(DSRC) $(XSRC)

clean:
	delete *.o *.sym mkci c.i /c/Ignore

# DO NOT DELETE THIS LINE - important for making dependencies!
#|.c|$*.o|
#|.a|$*.o|
#|.h|
# From this line on, everything has been created by MakeDepend.
# Anything you add yourself will be deleted automagically.

makefile.dev:
mkci.o: devproto.h dev.h device.h mkci.c
devio2.o: /syslog/syslog.h dev.h device.h devio2.c
devio1.o: devio1.a
device2.o: /syslog/syslog.h dev.h device.h device2.c
device1.o: device1.a
