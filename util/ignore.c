/*
 * $Id$
 *
 *  IGNORE.C
 *
 *  Makes it possible to ignore CRC errors.
 *
 *  This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
 */

#include "dev.h"
#include "device.h"


main(argc, argv)
int argc;
char **argv;
{
    struct MsgPort *port, *CreatePort();
    struct IOExtTD *tdreq, *CreateExtIO();
    UNIT *unit;
    long unitnr;
    int yesno;

    if (argc < 2) {
	Puts("Usage: ignore <unitnr> <YES/NO>\n");
	Puts("       If Yes, CRC errors will be ignored.\n");
	exit(1);
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
	if (tdreq = CreateExtIO(port, (long)sizeof(*tdreq))) {
	    OpenDevice("messydisk.device", unitnr, tdreq, 0L);
	    if (tdreq->iotd_Req.io_Device) {
		unit = (UNIT *)tdreq->iotd_Req.io_Unit;
		if (yesno != -42)
		    unit->mu_InitSectorStatus = yesno;
		else if (unit->mu_InitSectorStatus == CRC_UNCHECKED)
		    Puts("No\n");
		else
		    Puts("Yes\n");
		CloseDevice(tdreq);
	    } else
		Puts("Cannot OpenDevice messydisk\n");
	    DeleteExtIO(tdreq);
	} else
	    Puts("No memory for I/O request\n");
	DeletePort(port);
    } else
	Puts("No memory for replyport\n");
}

Puts(string)
char *string;
{
    long Output();

    Write(Output(), string, (long)strlen(string));
}

_wb_parse(){}
