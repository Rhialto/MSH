/*-
 * $Id: die.c,v 1.40 91/03/03 17:57:33 Rhialto Rel $
 *
 *  DIE.C
 *
 *  This code is (C) Copyright 1989-1991 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#include <amiga.h>
#include <functions.h>
#include <stdio.h>

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

    if (argc == 2) {
	if (filehandler = DeviceProc(argv[1])) {
	    dos_packet1(filehandler, ACTION_DIE, DOSTRUE);
	}
    } else
	printf("Usage: die DEV:\n");
}

