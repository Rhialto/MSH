/*-
 * $Id: die.c,v 1.55 1993/12/30 23:28:00 Rhialto Rel $
 *
 *  DIE.C
 *
 *  This code is (C) Copyright 1989-1992 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#include <stdio.h>
#include <string.h>
#include "han.h"

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif
#ifndef LIBRARIES_DOSEXTENS_H
#include <libraries/dosextens.h>
#endif

#ifndef CLIB_DOS_PROTOS_H
#include <clib/dos_protos.h>
#endif

const char	idString[] = "$\VER: Die $Revision: 1.55 $ $Date: 1993/12/30 23:28:00 $\r\n";

long
dos_packet2(struct MsgPort *port, long type, long arg1, long arg2)
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
    sp->sp_Pkt.dp_Arg2 = arg2;
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
    long	    flags = 0;
    long	    id = 0;

    if (argc == 3 && stricmp(argv[1], "unshare") == 0) {
	id = MSH_MAGIC;
	flags = 1;
	argc--;
	argv++;
    }
    if (argc == 3 && stricmp(argv[1], "unload") == 0) {
	id = MSH_MAGIC;
	flags = 1 | 2;
	argc--;
	argv++;
    }
    if (argc == 2) {
	if (filehandler = DeviceProc(argv[1])) {
	    dos_packet2(filehandler, ACTION_DIE, id, flags);
	}
    } else
	puts("Usage: die [unshare|unload] MSH-DEV:");

    return 0;
}
