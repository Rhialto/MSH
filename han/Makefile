# $Id: Makefile,v 1.53 92/10/25 03:31:10 Rhialto Rel $
#
# Makefile for messydos file system handler, for use with the
# DICE C compiler.
#
# This code is (C) Copyright 1989-1992 by Olaf Seibert. All rights reserved.
# May not be used or copied without a licence.

#DB =	 -DHDEBUG -I/syslog
#DBOBJ = /syslog/syslog.o
#WACK =  -s
REGARGS = -mRR -mi
PURE = -r -mS
HDR =	-Hhan.sym=han.h -Hdos.sym=dos.h
INC =	-I/han -I/dev -H/han/han.sym=/han/han.h -H/han/dos.sym=/han/dos.h

HOBJ =	hancrtso.o pack.o support.o hanmain.o hansec.o hanlock.o hanfile.o \
	hanconv.o hanreq.o hancmd.o date.o han2.o $(DBOBJ)
HSRC =	hancrtso.a pack.c support.c hanmain.c hansec.c hanlock.c hanfile.c \
	hanconv.c hanreq.c hancmd.c date.c han2.c dos.h han.h hanconv.h
XSRC =	Makefile /util/messyfmt.c /util/die.c \
	/util/setmain.c /util/setwindow.c /util/setwindow.h \
	/util/fmtmain.c /util/fmtwindow.c /util/fmtwindow.h

.SUFFIXES: .a

#   Options:
.c.o:
	dcc -c -proto -I/dev $(REGARGS) $(PURE) $(DB) $(HDR) $*.c -o $@

.a.o:
	a68k -q $*.a -o$@

all:	precompile /MessyFileSystem
utils:	precompile /c/Die /c/MessyFmt /c/LoadConv /c/MSH-Set /c/MSH-Format
doc:	dev.man msh.man

/MessyFileSystem:    hanproto.h $(HOBJ)
	dcc -o $@ -l0 $(REGARGS) $(PURE) $(WACK) $(HOBJ) -lc -lamiga20

hancrtso.o: hancrtso.a
	dcc -c -o hancrtso.o hancrtso.a

hanproto.h: $(HSRC)
	-delete hanproto.h han.sym
	makeproto -o hanproto.h $(HSRC)

# Dependencies for precompiled header files
han.sym: han.h
	-delete han.sym

dos.sym: dos.h
	-delete dos.sym

precompile: han.sym dos.sym

###

/c/Die: /util/die.c
	dcc -proto $(PURE) $(INC) /util/die.c -o /c/Die

/c/MessyFmt:   /util/MessyFmt.c /han/han.h
	dcc -proto $(PURE) $(INC) /util/messyfmt.c -o /c/MessyFmt

/c/LoadConv:   /util/LoadConv.c /han/han.h
	dcc -proto $(PURE) $(INC) /util/loadconv.c -o /c/LoadConv

###

/c/MSH-Set: /util/setmain.o /util/setwindow.o
	dcc /util/setmain.o /util/setwindow.o -o $@

/util/setmain.o: /util/setmain.c /util/setwindow.h
	dcc -c -proto $(DB) $(INC) /util/setmain.c -o /util/setmain.o

/util/setwindow.o: /util/setwindow.c /util/setwindow.h
	dcc -c -proto $(DB) $(INC)  /util/setwindow.c -o /util/setwindow.o

###

/c/MSH-Format: /util/fmtmain.o /util/fmtwindow.o
	dcc /util/fmtmain.o /util/fmtwindow.o -o $@

/util/fmtmain.o: /util/fmtmain.c /util/fmtwindow.h
	dcc -c -proto $(DB) $(INC) /util/fmtmain.c -o /util/fmtmain.o

/util/fmtwindow.o: /util/fmtwindow.c /util/fmtwindow.h
	dcc -c -proto $(DB) $(INC) $(HDR) /util/fmtwindow.c -o /util/fmtwindow.o

###

dev.man: dev.n
	nro dev.n >dev.man

msh.man: msh.n
	nro msh.n >msh.man

ci:
	RCS:ci -u $(RCSOPT) $(HSRC) $(XSRC)

co:
	RCS:co -l $(RCSOPT) $(HSRC) $(XSRC)

depend:
	mkd -I/syslog/ -I/dev/ -I/util/ -f $(HSRC) $(XSRC)

# DO NOT DELETE THIS LINE - important for making dependencies!
#|.c|$*.o|
#|.a|$*.o|
#|.h|
# From this line on, everything has been created by MakeDepend.
# Anything you add yourself will be deleted automagically.

/util/fmtmain.o: /util/fmtwindow.h  /dev/dev.h /han/han.h /util/fmtmain.c
/util/fmtwindow.o: /util/fmtwindow.h /util/fmtwindow.c
/util/setmain.o: /util/setwindow.h /han/hanconv.h  /dev/dev.h /han/han.h /util/setmain.c
/util/setwindow.o: /util/setwindow.h /util/setwindow.c
/util/die.o:  /dev/dev.h /han/han.h /util/die.c
/util/messyfmt.o:  /dev/dev.h /han/han.h /util/messyfmt.c
han2.o: /syslog/syslog.h dos.h	/dev/dev.h han.h han2.c
date.o: /syslog/syslog.h  /dev/dev.h han.h date.c
hancmd.o: /syslog/syslog.h hanconv.h  /dev/dev.h han.h hancmd.c
hanreq.o: /syslog/syslog.h dos.h  /dev/dev.h han.h hanreq.c
hanconv.o: /syslog/syslog.h hanconv.h  /dev/dev.h han.h hanconv.c
hanfile.o: /syslog/syslog.h hanconv.h dos.h  /dev/dev.h han.h hanfile.c
hanlock.o: /syslog/syslog.h dos.h  /dev/dev.h han.h hanlock.c
hansec.o: /syslog/syslog.h dos.h  /dev/dev.h han.h hansec.c
hanmain.o: /syslog/syslog.h hanconv.h dos.h  /dev/dev.h han.h hanmain.c
support.o: dos.h  /dev/dev.h han.h support.c
pack.o: /syslog/syslog.h dos.h	/dev/dev.h han.h pack.c
hancrtso.o: hancrtso.a
