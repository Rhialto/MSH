/*-
 * $Id: hancmd.c,v 1.55 1993/12/30 23:28:00 Rhialto Rel $
 * $Log: hancmd.c,v $
 * Revision 1.55  1993/12/30  23:28:00	Rhialto
 * Freeze for MAXON5.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:42:51  Rhialto
 * Add default conversion. Make ::M+ work as advertised.
 *
 * Revision 1.51  92/04/17  15:38:30  Rhialto
 * Freeze for MAXON3.
 *
 * Revision 1.46  91/10/06  18:25:10  Rhialto
 *
 * Freeze for MAXON
 *
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
 * This code is (C) Copyright 1990-1993 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include <stdlib.h>
#include "han.h"
#if CONVERSIONS
#include "hanconv.h"
#endif /* CONVERSIONS */

#if HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

Prototype void HandleCommand(char *cmd);
Local void	ltoa(unsigned long l, char *a);

Local int	DoMessages = 1;

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
    if (cmd[1] == 'B') {
	CheckBootBlock = atoi(&cmd[2]);
	if (DoMessages) {
	    static char msg[] = "BootBlk: 000";

	    ltoa(CheckBootBlock, msg + 9);
	    DisplayMessage(msg);
	}
#if CONVERSIONS
    } else if (cmd[1] == 'C') {
	DefaultConversion = cmd[2] & 31;
	if (DefaultConversion >= ConvFence)
	    DefaultConversion = ConvNone;
	if (DoMessages) {
	    static char msg[] = "Conversion: x";

	    msg[12] = '@' + DefaultConversion;
	    DisplayMessage(msg);
	}
#endif /* CONVERSIONS */
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
	DoMessages = (cmd[2] == '+')? 1 : atoi(&cmd[2]);
    }
}
