/*-
 * $Id: hancmd.c,v 1.45 91/10/04 00:12:32 Rhialto Exp $
 * $Log:	hancmd.c,v $
 * Revision 1.45  91/10/04  00:12:32  Rhialto
 * Add confirmation requesters and a switch for them
 * 
 * Revision 1.43  91/09/28  01:33:22  Rhialto
 * DICE conversion.
 *
 * Revision 1.42  91/06/13  23:44:47  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:55:08  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.32  90/11/23  23:50:53  Rhialto
 * Prepare for syslog
 *
 * Revision 1.30  90/06/04  23:18:03  Rhialto
 * Release 1 Patch 3
 *
 * HANCMD.C
 *
 * The code for the messydos file system handler
 *
 * Special commands through MSH::something file names.
 *
 * This code is (C) Copyright 1990,1991 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include <amiga.h>
#include <functions.h>
#include <stdlib.h>
#include "han.h"

#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

extern int	CheckBootBlock;
static void	ltoa(unsigned long l, char *a);

int		DoMessages = 1;

static void
ltoa(l, a)
unsigned long	l;
char	       *a;
{
    if (l > 999) {
	a[0] = '*';
	a[1] = '*';
	a[2] = '*';
    } else {
	a[0] = '0' + l / 100;
	l %= 100;
	a[1] = '0' + l / 10;
	l %= 10;
	a[2] = '0' + l;
    }
}

void
HandleCommand(cmd)
char	       *cmd;
{
#ifdef HDEBUG
    if (cmd[1] == 'D') {
    } else
#endif
    if (cmd[1] == 'B') {
	CheckBootBlock = atoi(&cmd[2]);
	if (DoMessages) {
	    static char msg[] = "BootBlk: 000";

	    ltoa(CheckBootBlock, msg + 9);
	    DisplayMessage(msg);
	}
    } else if (cmd[1] == 'F') {
	if (cmd[2] == '+') {
	    DiskIOReq->iotd_Req.io_Flags |= IOMDF_40TRACKS;
	    if (DoMessages) {
		DisplayMessage("40 track mode");
	    }
	} else if (cmd[2] == '-') {
	    DiskIOReq->iotd_Req.io_Flags &= ~IOMDF_40TRACKS;
	    if (DoMessages) {
		DisplayMessage("80 track mode");
	    }
	} else {
	    DiskIOReq->iotd_Req.io_Flags = atoi(&cmd[2]);
	    if (DoMessages) {
		static char msg[] = "io_Flags: 000";

		ltoa(DiskIOReq->iotd_Req.io_Flags, msg + 10);
		DisplayMessage(msg);
	    }
	}
    } else if (cmd[1] == 'M') {
	DoMessages = atoi(&cmd[2]);
    }
}

