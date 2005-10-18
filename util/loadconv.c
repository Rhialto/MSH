/*-
 * $Id: loadconv.c,v 1.58 2005/10/19 17:02:28 Rhialto Exp $
 *
 *  LOADCONV.C
 *
 *  This code is (C) Copyright 2005 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#include <stdio.h>
#include <string.h>
#include "han.h"
#undef CONVERSIONS
#define CONVERSIONS	1
#include "hanconv.h"

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

const char	idString[] = "$\VER: LoadConv $Revision: 1.58 $ $Date: 2005/10/19 17:02:28 $\r\n";

/*
 * Arg2 -> Arg2, the world turned upside down.
 */

long
dos_packet1a(struct MsgPort *port, long type, long arg2, long arg3)
{
    struct StandardPacket *sp;
    struct MsgPort *rp;

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
    sp->sp_Pkt.dp_Arg2 = arg2;
    sp->sp_Pkt.dp_Arg3 = arg3;
    PutMsg(port, &sp->sp_Msg);
    WaitPort(rp);
    GetMsg(rp);
    arg2 = sp->sp_Pkt.dp_Arg2;
    FreeMem(sp, (long)sizeof(*sp));
    DeletePort(rp);

    return arg2;
}

unsigned char *
CopyTable(unsigned char **ptr, unsigned char *init)
{
    Forbid();
    if (*ptr == 0) {
	*ptr = AllocMem(256L, 0L);
    }

    if (*ptr != 0) {
	CopyMem(init, *ptr, 256L);
    }
    Permit();

    return *ptr;
}

int
main(int argc, char **argv)
{
    struct MsgPort *filehandler;
    struct PrivateInfo *private;
    int 	    conversion;
    unsigned char   table[256];
    BPTR	    file;

#if VFATSUPPORT
    puts("Warning: Conversion syntax is not compatible with VFAT long file names.");
#endif

    if (argc != 4) {
	puts("Usage: loadconv MSH-DEV: C file\n\twhere C is the conversion id.");
	return 10;
    }

    filehandler = DeviceProc(argv[1]);
    if (filehandler == 0) {
	puts("Cannot find device.");
	return 10;
    }

    private = (struct PrivateInfo *)
	dos_packet1a(filehandler, ACTION_CURRENT_VOLUME, MSH_MAGIC, 1);

    if ((long)private == MSH_MAGIC ||
	private == 0 ||
	private->Revision != PRIVATE_REVISION ||
	private->Size != sizeof(*private)) {
	puts("Incompatible filesystem.");
	return 10;
    }
    puts(private->RCSId + 1);

    conversion = argv[2][0] & 31;
    if (conversion >= ConvFence)
	conversion = ConvNone;

    if (conversion == ConvNone ||
	conversion > private->NumConversions) {
	puts("Incorrect conversion type.");
	return 10;
    }

    conversion -= ConvNone + 1;     /* Make it 0-based */

    file = Open(argv[3], MODE_OLDFILE);
    if (file == 0) {
	puts("Cannot open file.");
	return 10;
    }

    if (Read(file, table, sizeof(table)) == 256)
	CopyTable(private->Table[conversion].to, table);

    if (Read(file, table, sizeof(table)) == 256)
	CopyTable(private->Table[conversion].from, table);

    Close(file);

    dos_packet1a(filehandler, ACTION_CURRENT_VOLUME, MSH_MAGIC, -1);

    return 0;
}
