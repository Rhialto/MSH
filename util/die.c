/*-
 * $Id: die.c,v 1.46 91/10/06 18:51:29 Rhialto Rel $
 *
 *  DIE.C
 *
 *  This code is (C) Copyright 1989-1992 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#include <amiga.h>
#include <functions.h>
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
	id = 'Msh\0';
	flags = 1;
	argc--;
	argv++;
    }
    if (argc == 3 && stricmp(argv[1], "unload") == 0) {
	id = 'Msh\0';
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
}
