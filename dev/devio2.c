/*-
 * $Id: devio2.c,v 1.44 91/10/02 21:07:42 Rhialto Exp $
 * $Log:	devio2.c,v $
 * Revision 1.44  91/10/02  21:07:42  Rhialto
 * Fix bug that sectors with number 0 are accepted and crash.
 * 
 * Revision 1.42  91/06/13  23:47:34  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:56:00  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.34  91/01/24  00:15:39  Rhialto
 * Use TD_RAWWRITE under AmigaOS 2.0.
 *
 * Revision 1.32  90/11/23  23:55:22  Rhialto
 * Prepare for syslog
 *
 * Revision 1.30  90/06/04  23:18:52  Rhialto
 * Release 1 Patch 3
 *
 * DEVIO.C
 *
 * The messydisk.device code that does the real work.
 *
 * This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
-*/

#include <amiga.h>
#include <functions.h>
#include "dev.h"
#include "device.h"

/*#undef DEBUG			/**/
#ifdef DEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif
#define REGISTER

struct DiskResource *DRResource;/* Argh! A global variable! */
void *CiaBResource;		/* And yet another! */

extern __far struct Custom custom;
extern __far struct CIA ciab;

__stkargs word DataCRC(byte *buffer);
__stkargs void IndexIntCode(void);
__stkargs void DskBlkIntCode(void);
int HardwareIO(DEV *dev, UNIT *unit, int dskwrite, struct IOStdReq *ioreq);
int DecodeTrack(DEV *dev, UNIT *unit);
__stkargs word DecodeTrack0(DEV *dev, UNIT *unit);
__stkargs void SafeEnableICR(long bits);
__stkargs byte DecodeByte(byte *mfmdecode, word mfm);

/*-
 *  The high clock bit in this table is still 0, but it could become
 *  a 1 if the two adjecent data bits are both 0.
 *  In fact, that is the principle of MFM clock bits: make sure that no
 *  two 1 bits are adjecent, but not too many (more than 3) 0 bits either.
 *  So, 0 c 0 -> 0 1 0	    (where c is a clock bit to be determined)
 *	0 c 1 -> 0 0 1
 *	1 c 0 -> 1 0 0
 *	1 c 1 -> 1 0 1
 *  The sync pattern, $4489, is %0100 0100 1000 1001
 *				  ~ ~  ~ ~  ~ ~  ~ ~ -> %1010 0001 -> $A1
 *  also follows the rules, but won't be formed by encoding $A1...
 *  Since the bytes are written high bit first, the unknown clock bit
 *  (for encoded nybbles 0-7, high bit 0) will become a 1 if the preceding
 *  byte was even (with low bit 0).
 *  So, the clock bit is the NOR of the two data bits.
-*/

byte		MfmEncode[16] = {
    0x2a, 0x29, 0x24, 0x25, 0x12, 0x11, 0x14, 0x15,
    0x4a, 0x49, 0x44, 0x45, 0x52, 0x51, 0x54, 0x55
};

#define SYNC	0x4489
#define TLEN	12500	    /* In BYTES */
#define RLEN	(TLEN+1324) /* 1 sector extra */
#define WLEN	(TLEN+20)   /* 20 bytes more than the theoretical track size */

#define INDEXGAP	40  /* All these values are in WORDS */
#define INDXGAP 	12
#define INDXSYNC	 3
#define INDXMARK	 1
#define INDEXGAP2	40
#define INDEXLEN	(INDEXGAP+INDXGAP+INDXSYNC+INDXMARK+INDEXGAP2)

#define IDGAP2		12  /* Sector header: 22 words */
#define IDSYNC		 3
#define IDMARK		 1
#define IDDATA		 4
#define IDCRC		 2
#define IDLEN		(IDGAP2+IDSYNC+IDMARK+IDDATA+IDCRC)

#define DATAGAP1	22  /* Sector itself: 552 words */
#define DATAGAP2	12
#define DATASYNC	 3
#define DATAMARK	 1
#define DATACRC 	 2
#define DATAGAP3_9	78  /* for 9 or less sectors/track */
#define DATAGAP3_10	40  /* for 10 sectors/track */
#define DATALEN 	(DATAGAP1+DATAGAP2+DATASYNC+DATAMARK+MS_BPS+DATACRC)

#define BLOCKLEN	(IDLEN+DATALEN)     /* Total: 574 words */

#define DSKDMAEN	(1<<15)
#define DSKWRITE	(1<<14)


/* INDENT ON */

int
HardwareIO(dev, unit, dskwrite, ioreq)
DEV	       *dev;
REGISTER UNIT  *unit;
int		dskwrite;
struct IOStdReq *ioreq;
{
    struct {
	struct Task *task;
	ulong signal;
    } tasksig;

    debug(("Disk buffer is at %lx\n", dev->md_Rawbuffer));

#ifndef NONCOMM
    if (dev->md_UseRawWrite && dskwrite) {
	REGISTER struct IOExtTD *tdreq = unit->mu_DiskIOReq;

	tdreq->iotd_Req.io_Command = TD_RAWWRITE;
	tdreq->iotd_Req.io_Flags = IOTDF_INDEXSYNC;
	tdreq->iotd_Req.io_Length = WLEN;
	tdreq->iotd_Req.io_Data = (APTR)dev->md_Rawbuffer;
	tdreq->iotd_Req.io_Offset = unit->mu_CurrentCylinder;
	if ((ioreq->io_Flags & IOMDF_40TRACKS) && (unit->mu_NumCyls == 80))
	    tdreq->iotd_Req.io_Offset *= 2;
	tdreq->iotd_Req.io_Offset *= NUMHEADS;
	tdreq->iotd_Req.io_Offset += unit->mu_CurrentSide;
	debug(("TDRawWrite %ld\n", tdreq->iotd_Req.io_Offset));
	MyDoIO((struct IORequest *)tdreq);

	return tdreq->iotd_Req.io_Error;
    }
#endif

    tasksig.task = FindTask(NULL);
    tasksig.signal = 1L << unit->mu_DmaSignal;

    unit->mu_DRUnit.dru_Index.is_Data = (APTR) ((WLEN >> 1)|DSKDMAEN| dskwrite);
    unit->mu_DRUnit.dru_DiscBlock.is_Data = (APTR) &tasksig;

    /* Clear signal bit */
    SetSignal(0L, tasksig.signal);

    /* Allocate drive and install index and block interrupts */
    GetDrive(&unit->mu_DRUnit);

    /* Clear any disk interrupts that might be pending (should not be needed) */
    custom.intreq = INTF_DSKBLK;

    /* Select correct drive and side */
    ciab.ciaprb = 0xff & ~CIAF_DSKMOTOR;    /* See hardware manual p229 */
    ciab.ciaprb = 0xff & ~CIAF_DSKMOTOR
		       & ~(CIAF_DSKSEL0 << unit->mu_UnitNr)
		       & ~(unit->mu_CurrentSide << CIAB_DSKSIDE);

    /* Set up disk parameters */

/*
 * This is the adkcon setup: MFM mode, wordsync, no MSBsync, fast mode.
 * The precomp is 0 nanoseconds for the outer half of the disk, 120 for
 * the rest.
 */
    {
	REGISTER word adk;

	custom.adkcon = ADKF_PRECOMP1|ADKF_PRECOMP0|ADKF_MSBSYNC;

	adk = ADKF_SETCLR|ADKF_MFMPREC|ADKF_FAST|ADKF_WORDSYNC;

	/* Are we on the inner half ? */
	if (unit->mu_CurrentCylinder > unit->mu_NumCyls >> 1) {
	    adk |= ADKF_PRECOMP0;
	}
	custom.adkcon = adk;
    }

    /* Set up disk buffer address */
    custom.dskpt = (APTR) dev->md_Rawbuffer;

    /* Enable disk DMA */
    custom.dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_DISK;

    if (dskwrite) {
	/* Enable disk index interrupt to start the whole thing. */
	SafeEnableICR(CIAICRF_FLG);
    } else {
	/* Set the sync word */
	custom.dsksync = SYNC;

	/* Do the same as the disk index interrupt would */
	custom.dsklen = DSKDMAOFF;
	custom.dsklen = (RLEN >> 1) | DSKDMAEN;
	custom.dsklen = (RLEN >> 1) | DSKDMAEN;

	custom.intena = INTF_SETCLR | INTF_DSKBLK;
    }

    Wait(tasksig.signal);

    FreeDrive();
    return TDERR_NoError;
}

#if 0
#define ID_ADDRESS_MARK     0xFE
#define MFM_ID		    0x5554
#define DATA_ADDRESS_MARK   0xFB
#define MFM_DATA	    0x5545

byte
DecodeByte(mfmdecode, mfm)
byte *mfmdecode;
word mfm;
{
    return mfmdecode[(byte)mfm & 0x7F] |
	   mfmdecode[(byte)(mfm >> 8) & 0x7F] << 4;
}

/*
/*#asm
mfmdecode   set     4
mfm	    set     8

_DecodeByte:
	move.l	mfmdecode(sp),a0
	move.b	mfm(sp),d1      ; high nybble
	and.w	#$7f,d1 	; strip clock bit (and garbage)
	move.b	(a0,d1.w),d0    ; decode 4 data bits
	lsl.b	#4,d0		; make room for the rest

	move.b	mfm+1(sp),d1    ; low nybble
	and.b	#$7f,d1 	; strip clock bit again
	or.b	(a0,d1.w),d0    ; insert 4 decoded bits

	rts

/*#endasm
*/

/* INDENT ON */

int
DecodeTrack(dev, unit)
DEV	       *dev;
UNIT	       *unit;
{
    REGISTER word  *rawbuf = (word *)dev->md_Rawbuffer; /*  a2 */
    word	   *rawend = (word *)
			     ((byte *)rawbuf + RLEN - (MS_BPS+2)*sizeof(word));
    byte	   *trackbuf = unit->mu_TrackBuffer;
    REGISTER byte  *decode = dev->md_MfmDecode; 	/*  a3 */
    word	   *oldcrc = unit->mu_CrcBuffer;
    REGISTER byte  *secptr;				/*  a4 */
    long	    sector;
    word	    numsecs;
    REGISTER long   numbytes;				/*  d3 */
    word	    maxsec;

#define Len	((byte *)rawbuf - dev->md_Rawbuffer)
    maxsec = 0;

    for (numsecs = 0; numsecs < MS_SPT_MAX; numsecs++) {
	/*
	 *  First try to find a sector id.
	 */
find_id:
	while (*rawbuf != SYNC) {
	    if (++rawbuf >= rawend) {
		debug(("id start, EOT %4lx\n", (long)Len));
		goto end;
	    }
	}
	while (*rawbuf == SYNC) {
	    rawbuf++;
	}
	if (*rawbuf++ != MFM_ID) {
	    debug(("No ID (%4lx), %4lx\n", (long)rawbuf[-1], (long)Len));
	    goto find_id;
	}

	sector = DecodeByte(decode, *rawbuf++);
	if (sector != unit->mu_CurrentCylinder) {
	    debug(("Cylinder error?? %ld\n", sector));
	    goto find_id;
	}
	sector = DecodeByte(decode, *rawbuf++);
	if (sector != unit->mu_CurrentSide) {
	    debug(("Side error?? %ld\n", sector));
	    goto find_id;
	}
	if (rawbuf >= rawend) {
	    debug(("id end, EOT %4lx\n", (long)Len));
	    goto end;
	}
	sector = DecodeByte(decode, *rawbuf++);
	debug(("#%2ld %4x, ", sector, (long)Len-0xC));
	if (sector > MS_SPT_MAX || sector < 1) {
	    debug(("Bogus sector number)\n"));
	    goto find_id;
	}
	if (sector > maxsec)
	    maxsec = sector;
	sector--;		/* Normalize sector number */

	/*
	 *  Then find the data block.
	 */
find_data:
	while (*rawbuf != SYNC) {
	    if (++rawbuf >= rawend) {
		debug(("data start, EOT %4lx\n", (long)Len));
		return 0; /* TDERR_TooFewSecs; */
	    }
	}
	while (*rawbuf == SYNC) {
	    rawbuf++;
	}
	if (*rawbuf++ != MFM_DATA) {
	    debug(("No Data (%4lx), %4lx\n", (long)rawbuf[-1], (long)Len));
	    goto find_id;
	}
	debug(("%4lx, ", (long)Len-8));

	if (rawbuf >= rawend) {
	    debug(("short data, EOT %4lx\n", (long)Len));
	    goto end;
	}
	secptr = trackbuf + MS_BPS * sector;
	for (numbytes = 0; numbytes < MS_BPS; numbytes++) {
	    *secptr++ = DecodeByte(decode, *rawbuf++);
	}
	debug(("%4lx\n", (long)Len));
	oldcrc[sector]	= DecodeByte(decode, *rawbuf++) << 8;
	oldcrc[sector] |= DecodeByte(decode, *rawbuf++);
	unit->mu_SectorStatus[sector] = unit->mu_InitSectorStatus;
    }

end:
    if (maxsec == 0)
	return TDERR_TooFewSecs;

#ifndef READONLY
    /*
     * If we read the very first track, we adjust our notion about the
     * number of sectors on each track. This is the only track we can
     * accurately find if this number is unknown. Let's hope that the first
     * user of this disk starts reading it here.
     */
    if (unit->mu_CurrentCylinder == 0 && unit->mu_CurrentSide == 0) {
	unit->mu_SectorsPerTrack = maxsec;
    }
    unit->mu_CurrentSectors = maxsec;
    debug(("%ld sectors\n", (long)unit->mu_SectorsPerTrack));
#endif

    return 0;

#undef Len
}
#else	/* Use assembly */

int
DecodeTrack(dev, unit)
DEV	       *dev;
UNIT	       *unit;
{
    word maxsec;

    maxsec = DecodeTrack0(dev, unit);

    if (maxsec == 0)
	return TDERR_TooFewSecs;

#ifndef READONLY
    /*
     * If we read the very first track, we adjust our notion about the
     * number of sectors on each track. This is the only track we can
     * accurately find if this number is unknown. Let's hope that the first
     * user of this disk starts reading it here.
     */
    if (unit->mu_CurrentCylinder == 0 && unit->mu_CurrentSide == 0) {
	unit->mu_SectorsPerTrack = maxsec;
    }
    unit->mu_CurrentSectors = maxsec;
    debug(("%ld sectors\n", (long)unit->mu_SectorsPerTrack));
#endif

    return 0;
}

#endif	/* using assembly */

/*
 * Initialize the ibm mfm decoding table
 */

void
InitDecoding(decode)
REGISTER byte  *decode;
{
    REGISTER int    i;

    i = 0;
    do {
	decode[i] = 0xff;
    } while (++i < 128);

    i = 0;
    do {
	decode[MfmEncode[i]] = i;
    } while (++i < 0x10);
}

#ifndef NONCOMM
long
MyDoIO(req)
REGISTER struct IORequest *req;
{
    req->io_Flags |= IOF_QUICK;
    BeginIO(req);
    return WaitIO(req);
}
#endif

/*
 * Switch the drive motor on. Return previous state. Don't use this when
 * you have allocated the disk via GetDrive().
 */

int
TDMotorOn(tdreq)
REGISTER struct IOExtTD *tdreq;
{
    debug(("TDMotorOn "));
    tdreq->iotd_Req.io_Command = TD_MOTOR;
    tdreq->iotd_Req.io_Length = 1;
    DoIO((struct IORequest *)tdreq);
    debug(("was %ld\n", tdreq->iotd_Req.io_Actual));

    return tdreq->iotd_Req.io_Actual;
}

/*
 * Get the number of cylinders the drive is capable of using.
 */

int
TDGetNumCyls(tdreq)
REGISTER struct IOExtTD *tdreq;
{
    tdreq->iotd_Req.io_Command = TD_GETNUMTRACKS;
    DoIO((struct IORequest *)tdreq);

    return tdreq->iotd_Req.io_Actual / NUMHEADS;
}

/*
 * Seek the drive to the indicated cylinder. Use the trackdisk.device for
 * ease. Don't use this when you have allocated the disk via GetDrive().
 */

int
TDSeek(unit, ioreq, cylinder)
UNIT	   *unit;
struct IOStdReq *ioreq;
int	    cylinder;
{

    REGISTER struct IOExtTD *tdreq = unit->mu_DiskIOReq;

    debug(("TDSeek %ld\n", (long)cylinder));

    tdreq->iotd_Req.io_Command = TD_SEEK;
    tdreq->iotd_Req.io_Offset = cylinder * (TD_SECTOR * NUMSECS * NUMHEADS);
    if ((ioreq->io_Flags & IOMDF_40TRACKS) && (unit->mu_NumCyls == 80))
	tdreq->iotd_Req.io_Offset *= 2;
    DoIO((struct IORequest *)tdreq);

    return tdreq->iotd_Req.io_Error;
}

void	       *
GetDrive(drunit)
REGISTER struct DiskResourceUnit *drunit;
{
    REGISTER void  *LastDriver;

    debug(("GetDrive: "));
    for (;;) {
	drunit->dru_Message.mn_Node.ln_Type = NT_MESSAGE;
	LastDriver = GetUnit(drunit);

	debug(("LastDriver %08lx\n", LastDriver));
	if (LastDriver) {
	    return LastDriver;
	} else {
	    while (drunit->dru_Message.mn_Node.ln_Type != NT_REPLYMSG)
		Wait(1L << drunit->dru_Message.mn_ReplyPort->mp_SigBit);
	    Remove(&drunit->dru_Message.mn_Node);
	    debug(("GetDrive: Retry\n"));
	}
    }
}

void
FreeDrive()
{
    GiveUnit();
}

int
GetTrack(ioreq, side, cylinder)
struct IOStdReq *ioreq;
int		side;
int		cylinder;
{
    REGISTER int    i;
    DEV 	   *dev;
    REGISTER UNIT  *unit;

    debug(("GetTrack %ld %ld\n", (long)cylinder, (long)side));
    dev = (DEV *) ioreq->io_Device;
    unit = (UNIT *) ioreq->io_Unit;

    if (cylinder != unit->mu_CurrentCylinder || side != unit->mu_CurrentSide) {
#ifndef READONLY
	Internal_Update(ioreq, unit);
#endif
	for (i = MS_SPT_MAX-1; i >= 0; i--) {
	    unit->mu_SectorStatus[i] = TDERR_NoSecHdr;
	}

	TDMotorOn(unit->mu_DiskIOReq);
	if (TDSeek(unit, ioreq, cylinder)) {
	    debug(("Seek error\n"));
	    return ioreq->io_Error = IOERR_BADLENGTH;
	}
	unit->mu_CurrentCylinder = cylinder;
	unit->mu_CurrentSide = side;
	ObtainSemaphore(&dev->md_HardwareUse);
	HardwareIO(dev, unit, 0, NULL); /* ioreq not needed */
	i = DecodeTrack(dev, unit);
	ReleaseSemaphore(&dev->md_HardwareUse);
	debug(("DecodeTrack returns %ld\n", (long)i));

	if (i != 0) {
	    unit->mu_CurrentCylinder = -1;
	    return i;
	}
    }

    return 0;
}

/*
 * Test if it is changed
 */

int
CheckChanged(ioreq, unit)
struct IOExtTD *ioreq;
REGISTER UNIT  *unit;
{
    REGISTER struct IOExtTD *tdreq;

    if ((ioreq->iotd_Req.io_Command & TDF_EXTCOM) &&
	ioreq->iotd_Count < unit->mu_ChangeNum) {
diskchanged:
	ioreq->iotd_Req.io_Error = TDERR_DiskChanged;
error:
	return 1;
    }
    return 0;
}

/*
 * Test if we can read or write the disk. Is it inserted and writable?
 */

int
CheckRequest(ioreq, unit)
struct IOExtTD *ioreq;
REGISTER UNIT  *unit;
{
    REGISTER struct IOExtTD *tdreq;

    if ((ioreq->iotd_Req.io_Command & TDF_EXTCOM) &&
	ioreq->iotd_Count < unit->mu_ChangeNum) {
diskchanged:
	ioreq->iotd_Req.io_Error = TDERR_DiskChanged;
error:
	return 1;
    }
    /*
     * if (ioreq->iotd_Req.io_Offset + ioreq->iotd_Req.io_Length >
     * (unit->mu_NumCyls * MS_NSIDES * MS_SPT * MS_BPS)) {
     * ioreq->iotd_Req.io_Error = IOERR_BADLENGTH; goto error; }
     */

    tdreq = unit->mu_DiskIOReq;

    if (unit->mu_DiskState == STATEF_UNKNOWN) {
	tdreq->iotd_Req.io_Command = TD_PROTSTATUS;
	DoIO((struct IORequest *)tdreq);
	if (tdreq->iotd_Req.io_Error == 0) {
	    if (tdreq->iotd_Req.io_Actual == 0) {
		unit->mu_DiskState = STATEF_PRESENT | STATEF_WRITABLE;
	    } else
		unit->mu_DiskState = STATEF_PRESENT;
	} else
	    unit->mu_DiskState = 0;
    }
    if (!(unit->mu_DiskState & STATEF_PRESENT))
	goto diskchanged;

    /*
     * Check _WRITE, _UPDATE, _FORMAT
     */
    if (STRIP(ioreq->iotd_Req.io_Command) != CMD_READ) {
	if (!(unit->mu_DiskState & STATEF_WRITABLE)) {
	    ioreq->iotd_Req.io_Error = TDERR_WriteProt;
	    goto error;
	}
    }
    return 0;
}


/*
 * Read zero or more sectors from the disk and copy them into the user's
 * buffer.
 */

void
CMD_Read(ioreq, unit)
REGISTER struct IOStdReq *ioreq;
REGISTER UNIT  *unit;
{
    int 	    side;
    int 	    cylinder;
    int 	    sector;
    byte	   *userbuf;
    long	    length;
    long	    offset;
    byte	   *diskbuf;
    int 	    retrycount;

    debug(("CMD_Read "));
    userbuf = (byte *) ioreq->io_Data;
    length = ioreq->io_Length / MS_BPS;        /* Sector count */
    offset = ioreq->io_Offset / MS_BPS;        /* Sector number */
    debug(("userbuf %08lx off %ld len %ld ", userbuf, offset, length));

    cylinder = offset / unit->mu_SectorsPerTrack;
    side = cylinder % MS_NSIDES;
    cylinder /= MS_NSIDES;
    sector = offset % unit->mu_SectorsPerTrack;       /* 0..8 or 9 */
    debug(("Tr=%ld Si=%ld Se=%ld\n", (long)cylinder, (long)side, (long)sector));

    ioreq->io_Actual = 0;

    if (length <= 0 || CheckRequest((struct IOExtTD *)ioreq, unit))
	goto end;

    retrycount = 0;
    diskbuf = unit->mu_TrackBuffer + MS_BPS * sector;
gettrack:
    GetTrack(ioreq, side, cylinder);

    for (;;) {
	/*
	 * Have we ever checked this CRC?
	 */
	if (unit->mu_SectorStatus[sector] == CRC_UNCHECKED) {
	    /*
	     * Do it now. If it mismatches, remember that for later.
	     */
	    if (unit->mu_CrcBuffer[sector] != DataCRC(diskbuf)) {
		debug(("%ld: %04lx, now %04lx\n", (long)sector, (long)unit->mu_CrcBuffer[sector], (long)DataCRC(diskbuf)));
		unit->mu_SectorStatus[sector] = TDERR_BadSecSum;
	    } else
		unit->mu_SectorStatus[sector] = TDERR_NoError;
	}
	if (unit->mu_SectorStatus[sector] > TDERR_NoError) {
	    if (++retrycount < 3) {
		unit->mu_CurrentCylinder = -1;
		goto gettrack;
	    }
	    ioreq->io_Error = unit->mu_SectorStatus[sector];
	    goto end;	    /* Don't use this sector anymore... */
	}
	retrycount = 0;
	CopyMem(diskbuf, userbuf, (long) MS_BPS);
	ioreq->io_Actual += MS_BPS;
	if (--length <= 0)
	    break;
	userbuf += MS_BPS;
	diskbuf += MS_BPS;
	if (++sector >= unit->mu_SectorsPerTrack) {
	    sector = 0;
	    diskbuf = unit->mu_TrackBuffer;
	    if (++side >= MS_NSIDES) {
		side = 0;
		if (++cylinder >= unit->mu_NumCyls) {
		    /* ioreq->io_Error = IOERR_BADLENGTH; */
		    goto end;
		}
	    }
	    GetTrack(ioreq, side, cylinder);
	}
    }

end:
    TermIO(ioreq);
}

#ifdef READONLY

void
CMD_Write(ioreq, unit)
REGISTER struct IOStdReq *ioreq;
UNIT	       *unit;
{
    ioreq->io_Error = TDERR_NotSpecified;
    TermIO(ioreq);
}

void
TD_Format(ioreq, unit)
REGISTER struct IOStdReq *ioreq;
UNIT	       *unit;
{
    ioreq->io_Error = TDERR_NotSpecified;
    TermIO(ioreq);
}

#endif

void
CMD_Reset(ioreq, unit)
struct IOStdReq *ioreq;
UNIT	       *unit;
{
    unit->mu_CurrentSide = -1;
    unit->mu_TrackChanged = 0;
    TermIO(ioreq);
}

void
CMD_Update(ioreq, unit)
struct IOStdReq *ioreq;
REGISTER UNIT  *unit;
{
#ifndef READONLY
    if (unit->mu_TrackChanged && !CheckRequest((struct IOExtTD *)ioreq, unit))
	Internal_Update(ioreq, unit);
#endif
    TermIO(ioreq);
}

void
CMD_Clear(ioreq, unit)
struct IOStdReq *ioreq;
UNIT	       *unit;
{
    if (!CheckChanged((struct IOExtTD *)ioreq, unit)) {
	unit->mu_CurrentSide = -1;
	unit->mu_TrackChanged = 0;
    }
    TermIO(ioreq);
}

void
TD_Seek(ioreq, unit)
struct IOStdReq *ioreq;
UNIT	       *unit;
{
    if (!CheckChanged((struct IOExtTD *)ioreq, unit)) {
	word		cylinder;

	cylinder = (ioreq->io_Offset / unit->mu_SectorsPerTrack) /
		    (MS_BPS * MS_NSIDES);
	TDSeek(unit, ioreq, cylinder);
    }
    TermIO(ioreq);
}

/*
 * Ask the trackdisk.device for the answer, but keep a local copy.
 */

void
TD_Changenum(ioreq, unit)
struct IOStdReq *ioreq;
UNIT	       *unit;
{
    REGISTER struct IOStdReq *req;

    req = &unit->mu_DiskIOReq->iotd_Req;
    req->io_Command = TD_CHANGENUM;
    DoIO((struct IORequest *)req);

    unit->mu_ChangeNum = req->io_Actual;
    ioreq->io_Actual = req->io_Actual;
    TermIO(ioreq);
}

int
DevInit(dev)
REGISTER DEV   *dev;
{
    if (!(DRResource = OpenResource(DISKNAME)))
	goto abort;

    if (!(CiaBResource = OpenResource(CIABNAME)))
	goto abort;

#ifndef READONLY
    if (!InitWrite(dev))
	goto abort;
#endif

    InitDecoding(dev->md_MfmDecode);
    InitSemaphore(&dev->md_HardwareUse);
    return 1;			/* Initializing succeeded */

abort:
    return DevCloseDown(dev);
}

int
DevCloseDown(dev)
DEV	       *dev;
{
#ifndef READONLY
    FreeBuffer(dev);
#endif
    return 0;			/* Now unitialized */
}

#ifndef READONLY
/*
 * Calculate the length between the sectors, given the length of the track
 * and the number of sectors that must fit on it.
 * The proper formula would be
 * (((TLEN/2) - INDEXLEN) / unit->mu_SectorsPerTrack) - BLOCKLEN;
 */

word
CalculateGapLength(sectors)
int		sectors;
{
    return (sectors == 10) ? DATAGAP3_10 : DATAGAP3_9;
}
#endif

UNIT	       *
UnitInit(dev, UnitNr)
DEV	       *dev;
ulong		UnitNr;
{
    REGISTER UNIT  *unit;
    struct Task    *task;
    struct IOStdReq *dcr;
    struct IOExtTD *tdreq;

    unit = AllocMem((long) sizeof (UNIT), MEMF_PUBLIC | MEMF_CLEAR);
    if (unit == NULL)
	return NULL;

    if (!(tdreq = CreateExtIO(&unit->mu_DiskReplyPort, (long) sizeof (*tdreq)))) {
	goto abort;
    }
    unit->mu_DiskIOReq = tdreq;
    if (OpenDevice(TD_NAME, UnitNr, (struct IORequest *)tdreq, TDF_ALLOW_NON_3_5)) {
	tdreq->iotd_Req.io_Device = NULL;
	goto abort;
    }
    if (tdreq->iotd_Req.io_Device->dd_Library.lib_Version >= SYS2_0)
	dev->md_UseRawWrite = 1;

    dcr = (void *) CreateExtIO(&unit->mu_DiskReplyPort, (long) sizeof (*dcr));
    if (dcr) {
	unit->mu_DiskChangeReq = dcr;
	unit->mu_DiskChangeInt.is_Node.ln_Pri = 32;
	unit->mu_DiskChangeInt.is_Data = (APTR) unit;
	unit->mu_DiskChangeInt.is_Code = DiskChangeHandler;
	/* Clone IO request part */
	dcr->io_Device = tdreq->iotd_Req.io_Device;
	dcr->io_Unit = tdreq->iotd_Req.io_Unit;
	dcr->io_Command = TD_ADDCHANGEINT;
	dcr->io_Data = (void *) &unit->mu_DiskChangeInt;
	SendIO((struct IORequest *)dcr);
    }
    NewList((struct List *)&unit->mu_ChangeIntList);

    unit->mu_NumCyls = TDGetNumCyls(tdreq);
    unit->mu_UnitNr = UnitNr;
    unit->mu_DiskState = STATEF_UNKNOWN;
    unit->mu_CurrentSide = -1;
    unit->mu_TrackChanged = 0;
    unit->mu_InitSectorStatus = CRC_UNCHECKED;
    unit->mu_SectorsPerTrack = MS_SPT;

    unit->mu_DRUnit.dru_Message.mn_ReplyPort = &unit->mu_DiskReplyPort;
    unit->mu_DRUnit.dru_Index.is_Node.ln_Pri = 32; /* high pri for index int */
    unit->mu_DRUnit.dru_Index.is_Code = IndexIntCode;
    unit->mu_DRUnit.dru_DiscBlock.is_Code = DskBlkIntCode;

    /*
     * Now create the Unit task. Remember that it won't start running
     * since we are Forbid()den. But just to be sure, we Forbid() again.
     */
    Forbid();
    task = CreateTask(DevName, TASKPRI, UnitTask, TASKSTACK);
    task->tc_UserData = (APTR) unit;

    unit->mu_Port.mp_Flags = PA_IGNORE;
    unit->mu_Port.mp_SigTask = task;
    NewList(&unit->mu_Port.mp_MsgList);

    unit->mu_DiskReplyPort.mp_Flags = PA_IGNORE;
    unit->mu_DiskReplyPort.mp_SigTask = task;
    NewList(&unit->mu_DiskReplyPort.mp_MsgList);
    Permit();

    return unit;

abort:
    UnitCloseDown(NULL, dev, unit);
    return NULL;
}

int
UnitCloseDown(ioreq, dev, unit)
struct IOStdReq *ioreq;
DEV	       *dev;
REGISTER UNIT  *unit;
{
    /*
     * Get rid of the Unit's task. We know this is safe because the unit
     * has an open count of zero, so it is 'guaranteed' not in use.
     */

    if (unit->mu_Port.mp_SigTask) {
	RemTask(unit->mu_Port.mp_SigTask);
    }
    if (unit->mu_DiskChangeReq) {
#if 0				/* V1.2 and V1.3 have a broken
				 * TD_REMCHANGEINT */
	REGISTER struct IOExtTD *req = unit->mu_DiskIOReq;

	req->iotd_Req.io_Command = TD_REMCHANGEINT;
	req->iotd_Req.io_Data = (void *) unit->mu_DiskChangeReq;
	DoIO(req);
	WaitIO(unit->mu_DiskChangeReq);
#else
	Disable();
	Remove(&unit->mu_DiskChangeReq->io_Message.mn_Node);
	Enable();
#endif
	DeleteExtIO((struct IORequest *)unit->mu_DiskChangeReq);
	unit->mu_DiskChangeReq = NULL;
    }
    if (unit->mu_DiskIOReq) {
	if (unit->mu_DiskIOReq->iotd_Req.io_Device)
	    CloseDevice((struct IORequest *)unit->mu_DiskIOReq);
	DeleteExtIO((struct IORequest *)unit->mu_DiskIOReq);
	unit->mu_DiskIOReq = NULL;
    }
    FreeMem(unit, (long) sizeof (UNIT));

    return 0;			/* Now unitialized */
}

/*
 * We handle disk change interrupts internally, since the io request is
 * held by the device. Since SoftInts caused by the trackdisk.device are
 * broadcast to our clients, our own softint must have the highest
 * priority possible.
 *
 * TD_Addchangeint is an IMMEDIATE command, so no exclusive use of the list
 * is acquired (nor released). The list is accessed by (software)
 * interrupt code.
 */

void
TD_Addchangeint(ioreq)
REGISTER struct IOStdReq *ioreq;
{
    REGISTER UNIT  *unit;

    unit = (UNIT *) ioreq->io_Unit;
    Disable();
    AddTail((struct List *)&unit->mu_ChangeIntList, &ioreq->io_Message.mn_Node);
    Enable();
    ioreq->io_Flags &= ~IOF_QUICK;	/* So we call ReplyMsg instead of
					 * TermIO */
    /* Note no TermIO */
}

void
TD_Remchangeint(ioreq)
REGISTER struct IOStdReq *ioreq;
{
    REGISTER struct IOStdReq *intreq;

    intreq = (struct IOStdReq *) ioreq->io_Data;
    Disable();
    Remove(&intreq->io_Message.mn_Node);
    Enable();
    ReplyMsg(&intreq->io_Message);      /* Quick bit always cleared */
    ioreq->io_Error = 0;
    TermIO(ioreq);
}

__stkargs __geta4 void
DiskChangeHandler0(unit)
UNIT	  *unit;
{
    REGISTER struct IOStdReq *ioreq;
    REGISTER struct IOStdReq *next;

    unit->mu_DiskState = STATEF_UNKNOWN;
    unit->mu_ChangeNum++;
    unit->mu_SectorsPerTrack = MS_SPT;
    for (ioreq = (struct IOStdReq *) unit->mu_ChangeIntList.mlh_Head;
	 next = (struct IOStdReq *) ioreq->io_Message.mn_Node.ln_Succ;
	 ioreq = next) {
	Cause((struct Interrupt *) ioreq->io_Data);
    }
}

#ifndef READONLY

/*
 * Parts of the following code were written by Werner Guenther.
 * Used with permission.
 */

/* mu_TrackChanged is a flag. When a sector has changed it changes to 1 */

/*
 * InitWrite() has to be called once at startup. It allocates the space
 * for one raw track, and writes the low level stuff between sectors
 * (gaps, syncs etc.)
 */

int
InitWrite(dev)
DEV	       *dev;
{
    if ((dev->md_Rawbuffer =
	    AllocMem((long)RLEN+2, MEMF_CHIP | MEMF_PUBLIC)) == 0)
	return 0;

    return 1;
}

/*
 * FreeBuffer has to be called when msh: closes down, it just frees the
 * memory InitWrite has allocated
 */

void
FreeBuffer(dev)
DEV	       *dev;
{
    if (dev->md_Rawbuffer) {    /* OIS */
	FreeMem(dev->md_Rawbuffer, (long) RLEN + 2);
    }
}

/*
 * This routine doesn't write to the disk, but updates the TrackBuffer to
 * respect the new sector. We have to be sure the TrackBuffer is filled
 * with the current Track. As GetSTS calls Internal_Update if the track
 * changes we don't have to bother about actually writing any data to the
 * disk. GetSTS has to be changed in the following way:
 *
 * if (cylinder != mu_CurrentCylinder || side != mu_CurrentSide) { Internal_Update(); for
 * (i = 0; i < MS_SPT; i++) ..... etc.
 */

void
CMD_Write(ioreq, unit)
REGISTER struct IOStdReq *ioreq;
UNIT	       *unit;
{
    int 	    side;
    int 	    cylinder;
    int 	    sector;
    byte	   *userbuf;
    long	    length;
    long	    offset;
    word	    spt;

    debug(("CMD_Write "));
    userbuf = (byte *) ioreq->io_Data;
    length = ioreq->io_Length / MS_BPS;        /* Sector count */
    offset = ioreq->io_Offset / MS_BPS;        /* Sector number */
    debug(("userbuf %08lx off %ld len %ld ", userbuf, offset, length));

    spt = unit->mu_SectorsPerTrack;
    cylinder = offset / spt;
    side = cylinder % MS_NSIDES;
    cylinder /= MS_NSIDES;
    sector = offset % spt;
    debug(("T=%ld Si=%ld Se=%ld\n", (long)cylinder, (long)side, (long)sector));

    ioreq->io_Actual = 0;

    if (length <= 0 || CheckRequest((struct IOExtTD *)ioreq, unit))
	goto end;

    GetTrack(ioreq, side, cylinder);
    for (;;) {
	CopyMem(userbuf, unit->mu_TrackBuffer + MS_BPS * sector, (long) MS_BPS);
	unit->mu_TrackChanged = 1;
	unit->mu_SectorStatus[sector] = CRC_CHANGED;

	ioreq->io_Actual += MS_BPS;
	if (--length <= 0)
	    break;
	userbuf += MS_BPS;
	/*
	 * Get next sequential sector/side/track
	 */
	if (++sector >= spt) {
	    sector = 0;
	    if (++side >= MS_NSIDES) {
		side = 0;
		if (++cylinder >= unit->mu_NumCyls)
		    goto end;
	    }
	    GetTrack(ioreq, side, cylinder);
	}
    }

    if (length)
	ioreq->io_Error = TDERR_NotSpecified;

end:
    TermIO(ioreq);
}

/*
 * This is called by your GetSTS() routine if the Track has changed. It
 * writes the changes back to the disk (a whole track at a time). It has
 * to be called if your device gets a CLOSE instruction too.
 */

void
Internal_Update(ioreq, unit)
struct IOStdReq *ioreq;
REGISTER UNIT  *unit;
{
    debug(("Internal_Update "));
    /* did we have a changed sector at all	 */
    if (unit->mu_TrackChanged != 0) {
	debug(("needs to write "));

	if (unit->mu_SectorsPerTrack > unit->mu_CurrentSectors)
	    unit->mu_CurrentSectors = unit->mu_SectorsPerTrack;

	/*
	 * Only recalculate the CRC on changed sectors. This way, a
	 * sector with a bad CRC won't suddenly be ``repaired''.
	 */
	{
	    REGISTER int i;

	    for (i = unit->mu_CurrentSectors - 1; i >= 0; i--) {
		if (unit->mu_SectorStatus[i] == CRC_CHANGED) {
		    unit->mu_CrcBuffer[i] = DataCRC(unit->mu_TrackBuffer + i * MS_BPS);
		    debug(("%ld: %04lx\n", (long)i, (long)unit->mu_CrcBuffer[i]));
		}
	    }
	}
	{
	    DEV 	   *dev;
	    REGISTER struct IOExtTD *tdreq;
	    word	    SectorGap;

	    dev = (DEV *) ioreq->io_Device;
	    tdreq = unit->mu_DiskIOReq;
	    SectorGap = CalculateGapLength(unit->mu_CurrentSectors);

	    ObtainSemaphore(&dev->md_HardwareUse);
	    EncodeTrack(unit->mu_TrackBuffer, dev->md_Rawbuffer,
			unit->mu_CrcBuffer,
			unit->mu_CurrentCylinder, unit->mu_CurrentSide,
			SectorGap, unit->mu_CurrentSectors);

	    TDMotorOn(tdreq);
	    if (TDSeek(unit, ioreq, unit->mu_CurrentCylinder)) {
		debug(("Seek error\n"));
		ioreq->io_Error = TDERR_SeekError;
		goto end;
	    }
	    HardwareIO(dev, unit, DSKWRITE, ioreq);

	    ReleaseSemaphore(&dev->md_HardwareUse);
	    unit->mu_TrackChanged = 0;
	}
    }
end:
    debug(("done\n"));
}

/*
 * TD_Format writes one or more whole tracks without reading them first.
 */

void
TD_Format(ioreq, unit)
REGISTER struct IOStdReq *ioreq;
UNIT	       *unit;
{
    REGISTER struct IOExtTD *tdreq = unit->mu_DiskIOReq;
    DEV 	   *dev;
    short	    side;
    int 	    cylinder;
    byte	   *userbuf;
    int 	    length;
    word	    spt;
    word	    gaplen;

    debug(("CMD_Format "));

    if (CheckRequest((struct IOExtTD *)ioreq, unit))
	goto termio;

    userbuf = (byte *) ioreq->io_Data;
    length = ioreq->io_Length / MS_BPS; 	   /* Sector count */
    cylinder = ioreq->io_Offset / MS_BPS;	   /* Sector number */
    /*
     * Now try to guess the number of sectors the user wants per track.
     * 40 sectors is the first ambiguous length.
     */
    if (length <= MS_SPT_MAX)
	spt = length;
    else if (length < 40) {
	if (length > 0) {
	    for (spt = 8; spt <= MS_SPT_MAX; spt++) {
		if ((length % spt) == 0)
		    goto found_spt;
	    }
	}
	/*
	 * Not 8, 16, 24, 32, 9, 18, 27, 36, 10, 20, or 30? That is an error.
	 */
	ioreq->io_Error = IOERR_BADLENGTH;
	goto termio;
    } else  /* assume previous number */
	spt = unit->mu_SectorsPerTrack;

found_spt:
    gaplen = CalculateGapLength(spt);

    /*
     * Assume the whole disk will have this layout.
     */
    unit->mu_SectorsPerTrack = spt;

    length /= spt;
    cylinder /= spt;

    side = cylinder % MS_NSIDES;
    cylinder /= MS_NSIDES;
    debug(("userbuf %08lx cylinder %ld len %ld\n", userbuf, (long)cylinder, (long)length));

    ioreq->io_Actual = 0;

    /*
     * Write out the current track if we are not going to overwrite it.
     * After the format operation, the buffer is invalidated.
     */
    if (cylinder <= unit->mu_CurrentCylinder &&
	unit->mu_CurrentCylinder < cylinder + length)
	Internal_Update(ioreq, unit);

    dev = (DEV *) ioreq->io_Device;

    while (length > 0) {
	{
	    REGISTER int i;

	    for (i = spt - 1; i >= 0; i--) {
		unit->mu_CrcBuffer[i] = DataCRC(userbuf + i * MS_BPS);
		debug(("%ld: %04x\n", (long)i, unit->mu_CrcBuffer[i]));
	    }
	}
	ObtainSemaphore(&dev->md_HardwareUse);
	EncodeTrack(userbuf, dev->md_Rawbuffer, unit->mu_CrcBuffer,
		    cylinder, side,
		    gaplen, spt);

	TDMotorOn(tdreq);
	if (TDSeek(unit, ioreq, cylinder)) {
	    debug(("Seek error\n"));
	    ioreq->io_Error = IOERR_BADLENGTH;
	    break;
	}
	unit->mu_CurrentSide = side;
	unit->mu_CurrentCylinder = cylinder;
	HardwareIO(dev, unit, DSKWRITE, ioreq);

	ReleaseSemaphore(&dev->md_HardwareUse);

	length--;
	userbuf += MS_BPS * spt;
	ioreq->io_Actual += MS_BPS * spt;

	if (++side >= MS_NSIDES) {
	    side = 0;
	    if (++cylinder >= unit->mu_NumCyls)
		goto end;
	}
    }
end:
    unit->mu_CurrentSide = -1;
    unit->mu_TrackChanged = 0;
termio:
    TermIO(ioreq);
}

#endif /* READONLY */
