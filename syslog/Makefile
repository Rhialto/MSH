# $Id: Makefile,v 1.1 1994/10/24 20:36:30 Rhialto Exp $
#
# Makefile for syslog programs and test

CC	= DCC -c -proto
CFLAGS	=
LARGE	= -mC -mD
INLINE	= -mi
LD	= DCC

.c.o:
	$(CC) $(CFLAGS) $< -o $@

all:	log syslogd syslogl.o syslogi.o syslogil.o
	@echo "All done."

syslog.o:   syslog.h syslog_private.h
syslogd.o:  syslog.h syslog_private.h

log:	log.o syslog.o
	$(LD) log.o syslog.o -o log

# syslog.o: large code, large data, long ints, no startup code

syslog.o:   syslog.c
	$(CC) $(CFLAGS) syslog.c

syslogl.o:   syslog.c
	$(CC) $(CFLAGS) $(LARGE) syslog.c -o syslogl.o

syslogi.o:   syslog.c
	$(CC) $(CFLAGS) $(INLINE) syslog.c -o syslogi.o

syslogil.o:   syslog.c
	$(CC) $(CFLAGS) $(INLINE) $(LARGE) syslog.c -o syslogil.o

syslogd: syslogd.o
	$(LD) syslogd.o -o syslogd
