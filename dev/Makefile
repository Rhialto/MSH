# $Id: Makefile.dev,v 1.53 92/10/25 02:19:20 Rhialto Rel $
#
# Makefile for messydos device, for use with the DICE C compiler.
#
# This code is (C) Copyright 1989-1992 by Olaf Seibert. All rights reserved.
# May not be used or copied without a licence.

#DBOBJ = /syslog/syslog.o
#DB =	 -DDEBUG
#WACK =  -s
REGARGS = -mRR
PURE = -mS
CLIB =	csr.lib  # when regargs are used set this to cr.lib
#CLIB = cs.lib	 # when no regargs are used set this to c.lib
HDR =	-Hdevice.sym=device.h

DOBJ =	device1.o device2.o devio1.o devio2.o $(DBOBJ)
DSRC =	device1.a device2.c devio1.a devio2.c dev.h device.h
XSRC =	mkci.c Makefile.dev /util/ignore.c

.SUFFIXES: .a

#   Options:
.c.o:
	dcc -c -proto $(REGARGS) $(PURE) $(DB) $(HDR) $*.c -o $@

.a.o:
	a68k -q $*.a -o$@

all:	/messydisk.device /c/Ignore

/messydisk.device: devproto.h $(DOBJ)
	dcc -o $@ -l0 $(REGARGS) $(WACK) $(DOBJ) $(CLIB) amigas13.lib

Mkci:	mkci.c dev.h device.h
	dcc mkci.c $(HDR) -o Mkci

/c/Ignore: /util/ignore.c dev.h device.h
	dcc -proto $(PURE) $(HDR) /util/ignore.c -o /c/Ignore

device1.o: device1.a c.i
devio1.o: devio1.a c.i

devproto.h: $(DSRC)
	makeproto -o devproto.h $(DSRC)
	-delete device.sym

device.sym: device.h
	delete device.sym

c.i:	mkci
	mkci c.i

ci:
	RCS:ci -u $(RCSOPT) $(DSRC) $(XSRC)

co:
	RCS:co -l $(RCSOPT) $(DSRC) $(XSRC)

depend:
	mkd -I/syslog/ -f $(DSRC) $(XSRC)

# DO NOT DELETE THIS LINE - important for making dependencies!
#|.c|$*.o|
#|.a|$*.o|
#|.h|
# From this line on, everything has been created by MakeDepend.
# Anything you add yourself will be deleted automagically.

makefile.dev:
mkci.o: devproto.h dev.h device.h mkci.c
devio2.o: /syslog/syslog.h devproto.h dev.h device.h devio2.c
devio1.o: devio1.a
device2.o: /syslog/syslog.h devproto.h dev.h device.h device2.c
device1.o: device1.a
