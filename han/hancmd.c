/*-
 * $Id: hancmd.c,v 1.1 90/03/11 18:10:50 Rhialto Rel $
 * $Log:	hancmd.c,v $
 * HANCMD.C
 *
 * The code for the messydos file system handler
 *
 * Special commands through MSH::something file names.
 *
 * This code is (C) Copyright 1990 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
-*/

#include "han.h"

#ifdef HDEBUG
#   define	debug(x)  dbprintf x
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
	extern short	DBEnable;

	DBEnable = name[2] & 0x0F;
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

