/*
 * $Id: ignore.c,v 1.56 1996/12/21 23:34:35 Rhialto Rel $
 *
 *  IGNORE.C
 *
 *  Makes it possible to ignore CRC errors.
 *
 *  This code is (C) Copyright 1989-1993 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
 */

#include "device.h"
#include <string.h>
#include <stdlib.h>

#ifndef CLIB_DOS_PROTOS_H
#include <clib/dos_protos.h>
#endif

const char	idString[] = "$""VER: Ignore $Revision: 1.56 $ $Date: 1996/12/21 23:34:35 $\r\n";

Puts(char *string)
{
    Write(Output(), string, (long)strlen(string));
}

int
main(int argc, char **argv)
{
    struct MsgPort *port;
    struct IOExtTD *tdreq;
    UNIT *unit;
    long unitnr;
    int yesno;
    int rc = 10;

    if (argc < 2) {
	Puts("Usage: ignore <unitnr> <YES/NO>\n");
	Puts("       If Yes, CRC errors will be ignored.\n");
	return rc;
    }

    unitnr = atoi(argv[1]);
    /*
     *	Don't be misled by the name CRC_UNCHECKED.
     *	It means the opposite happens.
     */
    if (argc > 2)
	yesno = ((argv[2][0] & 0x5F) == 'Y') ? TDERR_NoError : CRC_UNCHECKED;
    else
	yesno = -42;

    if (port = CreatePort(NULL, 0L)) {
	if (tdreq = (struct IOExtTD *)CreateExtIO(port, (long)sizeof(*tdreq))) {
	    OpenDevice("messydisk.device", unitnr, (struct IORequest *)tdreq,
		       0L);
	    if (tdreq->iotd_Req.io_Device) {
		unit = (UNIT *)tdreq->iotd_Req.io_Unit;
		if (yesno != -42) {
		    unit->mu_InitSectorStatus = yesno;
		    rc = 0;
		} else if (unit->mu_InitSectorStatus == CRC_UNCHECKED) {
		    Puts("No\n");
		    rc = 0;
		} else {
		    Puts("Yes\n");
		    rc = 5;
		}
		CloseDevice((struct IORequest *)tdreq);
	    } else
		Puts("Cannot OpenDevice messydisk\n");
	    DeleteExtIO((struct IORequest *)tdreq);
	} else
	    Puts("No memory for I/O request\n");
	DeletePort(port);
    } else
	Puts("No memory for replyport\n");

    return rc;
}
