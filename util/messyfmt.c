/*
 * $Id$
 * $Log$
 * MESSYFMT.C
 *
 * Formats a disk. Low-level formatting can also be done by mounting a file
 * system and using the AmigaDOS format command.
 *
 * This code is (C) Copyright 1989,1990 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
 */

#include <stdio.h>
#include "han.h"
extern int	Enable_Abort;

ulong		BootBlock[] = {
    0xEB349049, 0x424D2020, 0x332E3200, 0x02020100,	/* ...IBM  3.2..... */
    0x027000A0, 0x05F90300, 0x09000200, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x0000000F,
    0x00000000, 0x0100FA33, 0xC08ED0BC, 0x007C1607,
    0xBB780036, 0xC5371E56, 0x1653BF2B, 0x7CB90B00,
    0xFCAC2680, 0x3D007403, 0x268A05AA, 0x8AC4E2F1,
    0x061F8947, 0x02C7072B, 0x7CFBCD13, 0x7267A010,
    0x7C98F726, 0x167C0306, 0x1C7C0306, 0x0E7CA33F,
    0x7CA3377C, 0xB82000F7, 0x26117C8B, 0x1E0B7C03,
    0xC348F7F3, 0x0106377C, 0xBB0005A1, 0x3F7CE896,
    0x00B80102, 0xE8AA0072, 0x198BFBB9, 0x0B00BECD,
    0x7DF3A675, 0x0D8D7F20, 0xBED87DB9, 0x0B00F3A6,
    0x7418BE6E, 0x7DE86100, 0x32E4CD16, 0x5E1F8F04,
    0x8F4402CD, 0x19BEB77D, 0xEBEBA11C, 0x0533D2F7,
    0x360B7CFE, 0xC0A23C7C, 0xA1377CA3, 0x3D7CBB00,
    0x07A1377C, 0xE84000A1, 0x187C2A06, 0x3B7C4050,
    0xE84E0058, 0x72CF2806, 0x3C7C760C, 0x0106377C,
    0xF7260B7C, 0x03D8EBD9, 0x8A2E157C, 0x8A16FD7D,
    0x8B1E3D7C, 0xEA000070, 0x00AC0AC0, 0x7422B40E,
    0xBB0700CD, 0x10EBF233, 0xD2F73618, 0x7CFEC288,
    0x163B7C33, 0xD2F7361A, 0x7C88162A, 0x7CA3397C,
    0xC3B4028B, 0x16397CB1, 0x06D2E60A, 0x363B7C8B,
    0xCA86E98A, 0x16FD7D8A, 0x362A7CCD, 0x13C30D0A,
    0x4E6F6E2D, 0x53797374, 0x656D2064, 0x69736B20,	/* Non-System disk  */
    0x6F722064, 0x69736B20, 0x6572726F, 0x720D0A52,	/* or disk error..R */
    0x65706C61, 0x63652061, 0x6E642073, 0x7472696B,	/* eplace and strik */
    0x6520616E, 0x79206B65, 0x79207768, 0x656E2072,	/* e any key when r */
    0x65616479, 0x0D0A000D, 0x0A446973, 0x6B20426F,	/* eady.....Disk Bo */
    0x6F742066, 0x61696C75, 0x72650D0A, 0x0049424D,	/* ot failure...IBM */
    0x42494F20, 0x20434F4D, 0x49424D44, 0x4F532020,	/* BIO	COMIBMDOS   */
    0x434F4D00, 0x00000000, 0x00000000, 0x00000000,	/* COM............. */
    0x00000000, 0x00000000, 0x00000000, 0x000055AA,
};

byte	       *DiskTrack;
long		TrackSize;
int		Track;
int		LowTrack;
word		nsides;
struct IOExtTD *TDReq,
	       *CreateExtIO();
char	       *Device;

int
todigit(c)
register char	c;
{
    if ((c -= '0') < 0 || c > ('F' - '0'))
	return 42;

    if (c >= ('A' - '0')) {
	c -= 'A' - '9' - 1;
    }
    return c;
}

long
ntoi(str)
register char  *str;
{
    register long   total = 0;
    register long   value;
    register int    digit;
    register int    base;

    /* First determine the base */
number:
    if (*str == '0') {
	if (*++str == 'x') {    /* 0x means hexadecimal */
	    base = 16;
	    str++;
	} else {
	    base = 8;		/* Otherwise, 0 means octal */
	}
    } else {
	base = 10;		/* and any other digit means decimal */
    }

    value = 0;
    while ((digit = todigit(*str)) < base) {
	value *= base;
	value += digit;
	str++;
    }

suffix:
    switch (*str++) {
    case 'm':                   /* scale with megabytes */
	value *= 1024L * 1024;
	goto suffix;
    case 'k':                   /* scale with kilobytes */
	value *= 1024;
	goto suffix;
    case 's':                   /* scale with sectors */
	value *= TD_SECTOR;	/* or maybe even kilosectors! */
	goto suffix;
    case 'b':                   /* scale with bytes */
	goto suffix;
    }
    str--;

    total += value;

    if (*str >= '0' && *str <= '9')
	goto number;		/* Allow 10k512, recursion eliminated */

    return total;
}

word
input(question, defval)
char	       *question;
word		defval;
{
    char	    buf[80];

    printf("%s? [%d] ", question, defval);
    fflush(stdout);
    if (fgets(buf, sizeof (buf) - 1, stdin)) {
	if (buf[0] && buf[0] != '\n')
	    defval = ntoi(buf);
    }
    return defval;
}

void
PutWord(address, value)
register byte  *address;
register word	value;
{
    address[0] = value;
    address[1] = value >> 8;
}

word
SetWord(address, question, value)
byte	       *address;
char	       *question;
word		value;
{
    value = input(question, value);
    PutWord(address, value);

    return value;
}

byte
SetByte(address, question, value)
byte	       *address;
char	       *question;
word		value;
{
    value = input(question, value);
    return *address = value;
}

byte	       *
MaybeWrite(block)
byte	       *block;
{
    while (block >= (DiskTrack + TrackSize)) {
	int		t,
			s;

	t = Track / nsides;
	s = Track % nsides;
	printf("  Writing cylinder %3d side %d...\r", t, s);
	fflush(stdout);
	TDReq->iotd_Req.io_Command = TD_FORMAT;
	TDReq->iotd_Req.io_Data = (APTR) DiskTrack;
	TDReq->iotd_Req.io_Length = TrackSize;
	TDReq->iotd_Req.io_Offset = TrackSize * Track;
	DoIO(TDReq);
	if (TDReq->iotd_Req.io_Error) {
	    printf(" Write error %d on cylinder %d side %d.\n",
		   TDReq->iotd_Req.io_Error, t, s);
	}
	TDReq->iotd_Req.io_Command = CMD_UPDATE;
	DoIO(TDReq);
	if (TDReq->iotd_Req.io_Error) {
	    printf("Update error %d on cylinder %d side %d.\n",
		   TDReq->iotd_Req.io_Error, t, s);
	}
	TDReq->iotd_Req.io_Command = CMD_CLEAR;
	DoIO(TDReq);

	printf("  Read\r");
	fflush(stdout);
	TDReq->iotd_Req.io_Command = CMD_READ;
	TDReq->iotd_Req.io_Data = (APTR) DiskTrack;
	TDReq->iotd_Req.io_Length = TrackSize;
	TDReq->iotd_Req.io_Offset = TrackSize * Track;
	DoIO(TDReq);
	if (TDReq->iotd_Req.io_Error) {
	    printf("  Read error %d on cylinder %d side %d.\n",
		   TDReq->iotd_Req.io_Error, t, s);
	}
	setmem(DiskTrack, (int) TrackSize, 0);
	Track++;
	if ((block -= TrackSize) < DiskTrack)
	    block = DiskTrack;
    }
    return block;
}

main(argc, argv)
int		argc;
char	      **argv;
{
    struct MsgPort *port,
		   *CreatePort();
    byte	   *diskBlock;
    long	    unitNr;
    int 	    i;
    word	    bps = MS_BPS,
		    spt = MS_SPT;
    word	    res,
		    nfats,
		    spf,
		    nsects,
		    ncylinders,
		    ndirs,
		    wholeDisk,
		    clearFat,
		    endtrack;

    if (argc < 2) {
	printf("Usage: %s <unitnr> <device>\n"
	       "Formats a messydos volume in any desired shape.\n",
	       argv[0]);
	exit(1);
    }
    Enable_Abort = 0;
    unitNr = ntoi(argv[1]);
    if (argc > 2)
	Device = argv[2];
    else
	Device = "messydisk.device";

    if (!(port = CreatePort(NULL, 0L))) {
	puts("No memory for replyport");
	goto abort1;
    }
    if (!(TDReq = CreateExtIO(port, (long) sizeof (*TDReq)))) {
	puts("No memory for I/O request");
	goto abort2;
    }
    if (OpenDevice(Device, unitNr, TDReq, 0L)) {
	printf("Cannot OpenDevice %s\n", Device);
	goto abort3;
    }

    printf("Preparing to format disk in %s unit #%d.\n\n", Device, (int) unitNr);
    bps = input("Bytes per sector", bps);
    spt = input("Sectors per track", spt);
    TrackSize = bps * spt;
    nsides = input("Number of sides", MS_NSIDES);
    Track = input("Starting cylinder", 0);
    Track *= nsides;
    ncylinders = input("Number of cylinders", 80);
    endtrack = Track + nsides * ncylinders;

    if ((DiskTrack = AllocMem(TrackSize,
			MEMF_PUBLIC | MEMF_CHIP | MEMF_CLEAR)) == NULL) {
	puts("No memory for track buffer");
	goto abort4;
    }
    CopyMem(BootBlock, DiskTrack, (long) sizeof (BootBlock));

    PutWord(DiskTrack + 0x0b, bps);
    SetByte(DiskTrack + 0x0d, "Sectors per cluster", MS_SPC);
    res = SetWord(DiskTrack + 0x0e, "Bootsectors", MS_RES);
    nfats = SetByte(DiskTrack + 0x10, "Number of FAT copies", MS_NFATS);
    ndirs = SetWord(DiskTrack + 0x11, "Root directory entries", MS_NDIRS);
    nsects = SetWord(DiskTrack + 0x13, "Total number of sectors", spt * ncylinders * nsides);
    SetByte(DiskTrack + 0x15, "Media byte", 0xF9);
    spf = SetWord(DiskTrack + 0x16, "Sectors per FAT", MS_SPF);
    PutWord(DiskTrack + 0x18, spt);
    PutWord(DiskTrack + 0x1a, nsides);
    SetWord(DiskTrack + 0x1c, "Number of hidden sectors", 0);

    wholeDisk = input("Format whole disk (enter 1)", 0);
    if (!wholeDisk)
	clearFat = input("Write how much then?\n"
			 " (enter 0 for just the bootblock)\n"
			 " (enter 1 for FAT and root directory as well)", 0);

    if (input("Are you sure? (enter 42)", 0) != 42)
	goto abort5;

    if (Chk_Abort())
	goto abort5;

    if (!wholeDisk && !clearFat) {
	puts("Writing bootblock only.");
	TDReq->iotd_Req.io_Command = CMD_WRITE;
	TDReq->iotd_Req.io_Data = (APTR) DiskTrack;
	TDReq->iotd_Req.io_Length = sizeof (BootBlock);
	TDReq->iotd_Req.io_Offset = 0;
	DoIO(TDReq);
	TDReq->iotd_Req.io_Command = CMD_UPDATE;
	DoIO(TDReq);

	goto done;
    }

    /* Go to first FAT */
    diskBlock = MaybeWrite(DiskTrack + bps * res);
    for (i = 0; i < nfats; i++) {
	diskBlock[0] = 0xF9;
	diskBlock[1] = 0xFF;
	diskBlock[2] = 0xFF;
	diskBlock = MaybeWrite(diskBlock + bps * spf);  /* Next FAT */
    }

    /* Clear entire directory */
    diskBlock = MaybeWrite(diskBlock + ndirs * MS_DIRENTSIZE);
    MaybeWrite(DiskTrack + TrackSize);  /* Force a write */

    ncylinders *= nsides;
    if (wholeDisk) {
	while (Track < ncylinders) {
	    MaybeWrite(DiskTrack + TrackSize);  /* Write an empty track */
	    if (Chk_Abort())
		break;
	}
    }

done:
    TDReq->iotd_Req.io_Command = TD_MOTOR;
    TDReq->iotd_Req.io_Length = 0;
    DoIO(TDReq);

    printf("\n\nNow remove the disk from the drive (or use DiskChange).\n");

abort5:
    FreeMem(DiskTrack, TrackSize);
abort4:
    CloseDevice(TDReq);
abort3:
    DeleteExtIO(TDReq);
abort2:
    DeletePort(port);
abort1:;

}

_wb_parse()
{
}