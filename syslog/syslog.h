/*-
 * $Id: syslog.h,v 1.1 1994/10/24 20:38:56 Rhialto Exp $
 *
 * $Log: syslog.h,v $
 * Revision 1.1  1994/10/24  20:38:56  Rhialto
 * Initial revision
 *
 * DEBUGGING LOG DEAMON PUBLIC DEFINITIONS
-*/

#define Prototype   extern
#define Local	    static

#ifdef __STDC__
Prototype void initsyslog(void);
Prototype void uninitsyslog(void);
Prototype void syslog0(char *format, ...);
#endif

#define debug(args) syslog0 args
