/*-
 * $Id: die.c,v 1.55 1993/12/30 23:28:00 Rhialto Rel $
 *
 *  INHIBIT.C
 *
 *  This code is (C) Copyright 1989-1995 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#include <stdio.h>
#include <string.h>

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif
#ifndef LIBRARIES_DOSEXTENS_H
#include <libraries/dosextens.h>
#endif

#ifndef CLIB_EXEC_PROTOS_H
#include <clib/exec_protos.h>
#endif
#ifndef CLIB_ALIB_PROTOS_H
#include <clib/alib_protos.h>
#endif
#ifndef CLIB_DOS_PROTOS_H
#include <clib/dos_protos.h>
#endif

const char	idString[] = "$\VER: Inhibit $Revision: 1.55 $ $Date: 1993/12/30 23:28:00 $\r\n";

long
dos_packet1(struct MsgPort *port, long type, long arg1)
{
    struct StandardPacket *sp;
    struct MsgPort *rp;
    long res1;

    if ((rp = CreatePort(NULL, 0)) == NULL)
	return DOSFALSE;
    if ((sp = AllocMem((long)sizeof(*sp), MEMF_PUBLIC|MEMF_CLEAR)) == NULL) {
	DeletePort(rp);
	return DOSFALSE;
    }
    sp->sp_Msg.mn_Node.ln_Name = (char *)&sp->sp_Pkt;
    sp->sp_Pkt.dp_Link = &sp->sp_Msg;
    sp->sp_Pkt.dp_Port = rp;
    sp->sp_Pkt.dp_Type = type;
    sp->sp_Pkt.dp_Arg1 = arg1;
    PutMsg(port, &sp->sp_Msg);
    WaitPort(rp);
    GetMsg(rp);
    res1 = sp->sp_Pkt.dp_Res1;
    FreeMem(sp, (long)sizeof(*sp));
    DeletePort(rp);
    return res1;
}

int
main(int argc, char **argv)
{
    struct MsgPort *filehandler;
    long	    onoff = DOSTRUE;
    int		    rc = 0;

    if (argc == 3 && stricmp(argv[1], "on") == 0) {
	onoff = DOSTRUE;
	argc--;
	argv++;
    }
    if (argc == 3 && stricmp(argv[1], "off") == 0) {
	onoff = DOSFALSE;
	argc--;
	argv++;
    }
    if (argc == 2) {
	if (strchr(argv[1], ':') &&
	    (filehandler = DeviceProc(argv[1]))) {
	    onoff = dos_packet1(filehandler, ACTION_INHIBIT, onoff);
	    if (!onoff) {
		puts("Failed.");
		rc = 5;
	    }
	} else {
	    puts("Incorrect device.");
	    rc = 10;
	}
    } else {
	puts("Usage: Inhibit [on|off] DEV:");
    }

    return rc;
}
