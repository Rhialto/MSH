/*-
 * $Id$
 *
 *  DIE.C
 *
 *  This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 *  not be used or copied without a licence.
-*/

main(argc, argv)
int argc;
char **argv;
{
    struct MsgPort *filehandler, *DeviceProc();

    if (argc == 2) {
	if (filehandler = DeviceProc(argv[1])) {
	    dos_packet(filehandler, ACTION_DIE, DOSTRUE);
	}
    } else
	printf("Usage: die DEV:\n");
}

