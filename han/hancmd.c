/*-
 * $Id: hancmd.c,v 1.42 91/06/13 23:44:47 Rhialto Exp $
 * $Log:	hancmd.c,v $
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
 * This code is (C) Copyright 1990 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
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

void
HandleCommand(cmd)
register char  *cmd;
{
#ifdef HDEBUG
    if (cmd[1] == 'D') {
    } else
#endif
    if (cmd[1] == 'B') {
	CheckBootBlock = atoi(&cmd[2]);
    } else if (cmd[1] == 'F') {
	if (cmd[2] == '+')
	    DiskIOReq->iotd_Req.io_Flags |= IOMDF_40TRACKS;
	else if (cmd[2] == '-')
	    DiskIOReq->iotd_Req.io_Flags &= ~IOMDF_40TRACKS;
	else
	    DiskIOReq->iotd_Req.io_Flags = atoi(&cmd[2]);
    }
}

