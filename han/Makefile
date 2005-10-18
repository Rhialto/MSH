# $Id: Makefile,v 1.58 2005/10/19 16:53:52 Rhialto Exp $
#
# Makefile for messydos file system handler, for use with the # DICE C compiler.
#
# This code is (C) Copyright 1989-1997 by Olaf Seibert. All rights reserved.
# May not be used or copied without a licence.

#DBOBJ = /syslog/syslogi.o
#DB =	   -d2 -DHDEBUG=1 -I/syslog
#WACK =  -d2 -s
REGARGS = -mRR -mi
PURE = -r -mS
HDR =	-Hhan.sym=han.h -Hdos.sym=dos.h
HDRL =	-Hhanl.sym=han.h -Hdosl.sym=dos.h
INC =	-I/han -I/dev -H/han/han.sym=/han/han.h -H/han/dos.sym=/han/dos.h
RCSVER = -r1.58

HOBJ =	hancrtso.o pack.o support.o hanmain.o hansec.o hanlock.o hanfile.o \
	hanconv.o hanreq.o hancmd.o date.o han2.o hanvfat.o $(DBOBJ)
HLOBJ = hancrtso.o pack.ol support.o hanmain.ol hansec.ol hanlock.ol hanfile.ol \
	hanconv.o hanreq.o hancmd.o date.o han2.ol $(DBOBJ)
HSRC =	hancrtso.a pack.c support.c hanmain.c hansec.c hanlock.c hanfile.c \
	hanconv.c hanreq.c hancmd.c date.c han2.c hanvfat.c dos.h han.h \
	hanconv.h
XSRC =	Makefile /util/messyfmt.c /util/die.c /util/inhibit.c \
	/util/setmain.c /util/setwindow.c /util/setwindow.h \
	/util/fmtmain.c /util/fmtwindow.c /util/fmtwindow.h \
	/util/loadconv.c /util/ignore.c
DOC =	/doc/msh.texi

.SUFFIXES: .a .guide .texi

#   Options:
.c.o:
	dcc -c -proto -I/dev $(REGARGS) $(PURE) $(DB) $(HDR) $*.c -o $@

.c.ol:
	dcc -c -proto -I/dev $(REGARGS) $(PURE) $(DB) $(HDRL) -DLONGNAMES=1 $*.c -o $@

.a.o:
	a68k -q $*.a -o$@

.texi.guide:
	-makeinfo --amiga-39 $*.texi

it:	fs
all:	fs utils doc

fs:	precompile /MessyFileSystem # /LMFileSystem
utils:	precompile /c/Die /c/Inhibit /c/MessyFmt /c/LoadConv \
	/c/MSH-Set /c/MSH-Format /c/Ignore
doc:	/doc/dev.man /doc/msh.guide

/MessyFileSystem:    hanproto.h $(HOBJ)
	dcc -o $@ -l0 $(REGARGS) $(PURE) $(WACK) $(HOBJ) -lc -lamiga30

/LMFileSystem:	  hanproto.h $(HLOBJ)
	dcc -o $@ -l0 $(REGARGS) $(PURE) $(WACK) $(HLOBJ) -lc -lamiga30

hancrtso.o: hancrtso.a
	dcc -c -o hancrtso.o hancrtso.a

#   Delete the precompiled headers when hanproto.h is generated,
#   because it is included there.

hanproto.h: # $(HSRC)
	-delete hanproto.h han.sym hanl.sym
	makeproto -o hanproto.h $(HSRC)

#   Dependencies for precompiled header files

han.sym: han.h
	-delete han.sym

hanl.sym: han.h
	-delete hanl.sym

dos.sym: dos.h
	-delete dos.sym

dosl.sym: dos.h
	-delete dosl.sym

precompile: han.sym hanl.sym dos.sym dosl.sym

clean:
	-delete *.sym *.o *.ol

spotless:   clean
	-delete /c/Die /c/Inhibit /c/MessyFmt /c/LoadConv \
	    /c/MSH-Set /c/MSH-Format /c/Ignore

###

/c/Die: /util/die.c
	dcc -proto $(PURE) $(INC) /util/die.c -o /c/Die

/c/Ignore: /util/Ignore.c
	dcc -proto $(PURE) $(INC) /util/Ignore.c -o /c/Ignore

/c/Inhibit: /util/Inhibit.c
	dcc -proto $(PURE) $(INC) /util/Inhibit.c -o /c/Inhibit

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
	dcc -c -proto $(DB) $(INC) /util/setwindow.c -o /util/setwindow.o

###

/c/MSH-Format: /util/fmtmain.o /util/fmtwindow.o
	dcc /util/fmtmain.o /util/fmtwindow.o -o $@

/util/fmtmain.o: /util/fmtmain.c /util/fmtwindow.h
	dcc -c -proto $(DB) $(INC) /util/fmtmain.c -o /util/fmtmain.o

/util/fmtwindow.o: /util/fmtwindow.c /util/fmtwindow.h
	dcc -c -proto $(DB) $(INC) $(HDR) /util/fmtwindow.c -o /util/fmtwindow.o

###

/doc/dev.man: /doc/dev.n
	nro /doc/dev.n >/doc/dev.man

/doc/msh.man: /doc/msh.n
	nro /doc/msh.n >/doc/msh.man

ci:
	RCS:ci -f -u $(RCSVER) $(HSRC) $(XSRC) $(DOC)

co:
	RCS:co -u $(HSRC) $(XSRC) $(DOC)

depend:
	mkd -I/syslog/ -I/dev/ -I/util/ -f $(HSRC) $(XSRC)

# DO NOT DELETE THIS LINE - important for making dependencies!
#|.c|$*.o|
#|.a|$*.o|
#|.h|
# From this line on, everything has been created by MakeDepend.
# Anything you add yourself will be deleted automagically.

/util/ignore.o: /dev/device.h /util/ignore.c
/util/loadconv.o: hanconv.h hanproto.h /dev/dev.h han.h /util/loadconv.c
/util/fmtwindow.o: /util/fmtwindow.h /util/fmtwindow.c
/util/fmtmain.o: /util/fmtwindow.h hanproto.h /dev/dev.h han.h /util/fmtmain.c
/util/setwindow.o: /util/setwindow.h /util/setwindow.c
/util/setmain.o: /util/setwindow.h hanconv.h hanproto.h /dev/dev.h han.h /util/setmain.c
/util/inhibit.o: /util/inhibit.c
/util/die.o: hanproto.h /dev/dev.h han.h /util/die.c
/util/messyfmt.o: hanproto.h /dev/dev.h han.h /util/messyfmt.c
hanvfat.o: /syslog/syslog.h dos.h hanproto.h /dev/dev.h han.h hanvfat.c
han2.o: /syslog/syslog.h dos.h hanproto.h /dev/dev.h han.h han2.c
date.o: /syslog/syslog.h hanproto.h /dev/dev.h han.h date.c
hancmd.o: /syslog/syslog.h hanconv.h hanproto.h /dev/dev.h han.h hancmd.c
hanreq.o: /syslog/syslog.h dos.h hanproto.h /dev/dev.h han.h hanreq.c
hanconv.o: /syslog/syslog.h hanconv.h hanproto.h /dev/dev.h han.h hanconv.c
hanfile.o: /syslog/syslog.h hanconv.h dos.h hanproto.h /dev/dev.h han.h hanfile.c
hanlock.o: /syslog/syslog.h dos.h hanproto.h /dev/dev.h han.h hanlock.c
hansec.o: /syslog/syslog.h dos.h hanproto.h /dev/dev.h han.h hansec.c
hanmain.o: /syslog/syslog.h hanconv.h dos.h hanproto.h /dev/dev.h han.h hanmain.c
support.o: /syslog/syslog.h dos.h hanproto.h /dev/dev.h han.h support.c
pack.o: /syslog/syslog.h dos.h hanproto.h /dev/dev.h han.h pack.c
hancrtso.o: hancrtso.a
