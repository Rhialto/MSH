/*-
 * $Id: log.c,v 1.1 1994/10/24 20:36:30 Rhialto Exp $
 *
 * $Log: log.c,v $
 * Revision 1.1  1994/10/24  20:36:30  Rhialto
 * Initial revision
 *
 * LOG A MESSAGE
-*/

#include "syslog.h"

main(argc, argv)
int		argc;
char	      **argv;
{
    initsyslog();
    syslog0("%s: %s\n", argv[0], argv[1]);
    uninitsyslog();
}
