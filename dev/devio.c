/*-
 * $Id: devio.c,v 1.34 91/01/24 00:15:39 Rhialto Exp $
 * $Log:	devio.c,v $
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

#include "dev.h"
#include "device.h"

/*#undef DEBUG			/**/
#ifdef DEBUG
#   define	debug(x)  syslog x
#else
#   define	debug(x)
#endif

struct DiskResource *DRResource;/* Argh! A global variable! */
void *CiaBResource;		/* And yet another! */

void		Internal_Update();
word		DataCRC();
word		CalculateGapLength();

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

/* INDENT OFF */
#asm

; Some hardware data:

INDEXSYNC	equ $5224	; special sync for index
SYNC		equ $4489	; normal sector sync
TLEN		equ 12500	; 2 miscrosecs/bit, 200 ms/track -> 100000 bits
WLEN		equ TLEN+20

;;;;
;
;   The following lengths are all in unencoded bytes (or encoded words)

INDEXGAP	equ 40
INDXGAP 	equ 12
INDXSYNC	equ  3
INDXMARK	equ  1
INDEXGAP2	equ 40

IDGAP2		equ 12
IDSYNC		equ  3
IDMARK		equ  1
IDDATA		equ  4
IDCRC		equ  2

DATAGAP1	equ 22
DATAGAP2	equ 12
DATASYNC	equ  3
DATAMARK	equ  1
DATACRC 	equ  2

custom		equ $DFF000

Dsklen		equ $24
Intena		equ $9a 	; Interrupt enable  register (write)
Intreq		equ $9c 	; Interrupt request register (write)

; Flags in DSKLEN:

dskdmaoff	equ $4000

; Flags in INTENA/INTREQ:

intf_setclr	equ 1<<15
intf_dskblk	equ 1<<1

; CIA interrupt control register bits/flags:

ciaicrf_flg	equ    1<<4	 ; flg interrupt (disk index)

; some cia.resource library functions

		public	_LVOSignal
		public	_LVOAbleICR
		public	_LVOSetICR

_SafeEnableICR: move.l	_CiaBResource,a6
		move.b	4+1(sp),d0
		jsr	_LVOSetICR(a6)      ; clear pending interrupt
		move.b	4+1(sp),d0
		or.b	#$80,d0 	    ; then enable it
		jsr	_LVOAbleICR(a6)
		rts
;;;;
;
;   Disk index interrupt code.
;   is_Data (A1) is the value to stuff into the DSKLEN register.
;   A0 points to the custom chips already.
;   It then enables the disk block interrupt and disables the
;   index interrupt.

_IndexIntCode:
;	 movem.l A2-A4/D2-D7,-(sp)
	move.w	#dskdmaoff,Dsklen(a0)
	move.w	a1,Dsklen(a0)
	move.w	a1,Dsklen(a0)       ; this enables the DMA
	move.w	#intf_setclr|intf_dskblk,Intena(a0)
	move.l	_CiaBResource,a6
	move.b	#ciaicrf_flg,d0
	jsr	_LVOAbleICR(a6)     ; disable index interrupt
;	 movem.l (sp)+,A2-A4/D2-D7
	rts
;;;;
;
;   Disk DMA finished interrupt code.
;   (a1) is the task to Signal, 4(a1) is the signal mask to use.
;   Disables the disk block finished interrupt.

_DskBlkIntCode:
	move.w	#dskdmaoff,Dsklen(a0)   ; disable disk DMA
	move.w	#intf_dskblk,Intena(a0) ; disable the interrupt
	move.w	#intf_dskblk,Intreq(a0) ; clear 'diskblock finished' flag
	move.l	4(a1),d0            ; signal mask
	move.l	(a1),a1             ; task to signal
	jsr	_LVOSignal(a6)
	rts

#endasm

#define DSKDMAEN	(1<<15)
#define DSKWRITE	(1<<14)

void IndexIntCode(), DskBlkIntCode();

/* INDENT ON */

int
HardwareIO(dev, unit, dskwrite, ioreq)
DEV	       *dev;
register UNIT  *unit;
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
	register struct IOExtTD *tdreq = unit->mu_DiskIOReq;

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
	MyDoIO(tdreq);

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
	register word adk;

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
	SafeEnableICR((int) CIAICRF_FLG);
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

/* INDENT OFF
byte
DecodeByte(mfmdecode, mfm)
byte *mfmdecode;
word mfm;
{
    return mfmdecode[(byte)mfm & 0x7F] |
	   mfmdecode[(byte)(mfm >> 8) & 0x7F] << 4;
} */

#asm
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

#endasm

/* INDENT ON */
byte DecodeByte();

int
DecodeTrack(dev, unit)
DEV	       *dev;
UNIT	       *unit;
{
    register word  *rawbuf = (word *)dev->md_Rawbuffer; /*  a2 */
    byte	   *rawend = (byte *)rawbuf + RLEN - (MS_BPS+2)*sizeof(word);
    byte	   *trackbuf = unit->mu_TrackBuffer;
    register byte  *decode = dev->md_MfmDecode; 	/*  a3 */
    word	   *oldcrc = unit->mu_CrcBuffer;
    register byte  *secptr;				/*  a4 */
    long	    sector;
    word	    numsecs;
    register long   numbytes;				/*  d3 */
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
	if (sector > MS_SPT_MAX) {
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
    if (numsecs == 0)
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
    register word  *rawbuf = (word *)dev->md_Rawbuffer; /*  a2 */
    byte	   *rawend = (byte *)rawbuf + RLEN - (MS_BPS+2)*sizeof(word);
    byte	   *trackbuf = unit->mu_TrackBuffer;
    register byte  *decode = dev->md_MfmDecode; 	/*  a3 */
    word	   *oldcrc = unit->mu_CrcBuffer;
    register byte  *secptr;				/*  a4 */
    long	    sector;
    word	    numsecs;
    register long   numbytes;				/*  d3 */
    word	    maxsec;

#asm

MFM_ID	    equ     $5554
MFM_DATA    equ     $5545

rawbuf	    equr    a2
decode	    equr    a3
secptr	    equr    a4
numbytes    equr    d3

rawend	    set     -4
trackbuf    set     -8
oldcrc	    set     -12
sector	    set     -16
numsecs     set     -18
maxsec	    set     -20

	move.w	#0,numsecs(a5)      ; no sectors found yet
	move.w	#0,maxsec(a5)       ; and no highest sector number

;;;;	First we will try to find a sector id.
find_id:
	cmp	#SYNC,(rawbuf)+
	beq.s	fid_gotsync
	cmpa.l	rawend(a5),rawbuf
	blt	find_id
	bra	return		    ; We ran off the end of the buffer.

fid_gotsync:			    ; Skip the other syncs.
	cmp.w	#SYNC,(rawbuf)
	bne	fid_endsync
	lea	2(rawbuf),rawbuf
	bra	fid_gotsync

fid_endsync:
	cmp.w	#MFM_ID,(rawbuf)+
	bne	find_id

	bsr	DecodeByte	    ; cylinder #
	bsr	DecodeByte	    ; side #
	moveq.l #0,d0		    ; clear high part
	bsr	DecodeByte	    ; sector #
	cmp.w	#MS_SPT_MAX,d0	    ; sector number too large?
	bgt	find_id
	cmp.w	maxsec(a5),d0       ; what is the highest sector number?
	ble	nomax
	move.w	d0,maxsec(a5)       ; record the highest sector number
nomax:
	subq.w	#1,d0		    ; normalize sector number
	move.l	d0,sector(a5)

find_data:			    ; Then find the data block.
	cmp	#SYNC,(rawbuf)+
	beq.s	fda_gotsync
	cmpa.l	rawend(a5),rawbuf
	blt	find_data
	bra	return		    ; we ran off the end of the buffer.

fda_gotsync:			    ; skip the other syncs.
	cmp.w	#SYNC,(rawbuf)
	bne	fda_endsync
	lea	2(rawbuf),rawbuf
	bra	fda_gotsync

fda_endsync:
	cmp.w	#MFM_DATA,(rawbuf)+ ; do we really have a data block?
	bne	find_id

	cmpa.l	rawend(a5),rawbuf   ; will we still be inside the mfm data?
	bge	return

	move.l	sector(a5),d0       ; calculate the location to
	moveq.l #LOG2_MS_BPS,d1     ;  store this sector.
	asl.l	d1,d0
	move.l	trackbuf(a5),secptr
	add.l	d0,secptr

	move.w	#MS_BPS-1,numbytes
data_copy:
	bsr	DecodeByte
	move.b	d0,(secptr)+
	dbra	numbytes,data_copy

	move.l	sector(a5),d3       ; get pointer to crc location
	add.l	d3,d3		    ; 2 bytes of crc per sector
	move.l	oldcrc(a5),a0
	add.l	d3,a0

	bsr	DecodeByte	    ; get high byte
	move.b	d0,(a0)+
	bsr	DecodeByte	    ; and low byte of crc
	move.b	d0,(a0)+

#endasm
	unit->mu_SectorStatus[sector] = unit->mu_InitSectorStatus;
#asm
	addq.w	#1,numsecs(a5)
	cmp.w	#MS_SPT_MAX,numsecs(a5)
	blt	find_id
return:
#endasm

    if (numsecs == 0)
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

#asm
;;;;
;
;   Decode a single MFM word to a byte.
;   Auto-increments the rawbuffer pointer.

DecodeByte:
	move.b	(rawbuf)+,d1    ; high nybble
	and.w	#$7f,d1 	; strip clock bit (and garbage)
	move.b	(decode,d1.w),d0; decode 4 data bits
	lsl.b	#4,d0		; make room for the rest

	move.b	(rawbuf)+,d1    ; low nybble
	and.b	#$7f,d1 	; strip clock bit again
	or.b	(decode,d1.w),d0; insert 4 decoded bits

	rts

#endasm
#endif	/* using assembly */

/*
 * Initialize the ibm mfm decoding table
 */

void
InitDecoding(decode)
register byte  *decode;
{
    register int    i;

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
register struct IORequest *req;
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
register struct IOExtTD *tdreq;
{
    debug(("TDMotorOn "));
    tdreq->iotd_Req.io_Command = TD_MOTOR;
    tdreq->iotd_Req.io_Length = 1;
    DoIO(tdreq);
    debug(("was %ld\n", tdreq->iotd_Req.io_Actual));

    return tdreq->iotd_Req.io_Actual;
}

/*
 * Get the number of cylinders the drive is capable of using.
 */

int
TDGetNumCyls(tdreq)
register struct IOExtTD *tdreq;
{
    tdreq->iotd_Req.io_Command = TD_GETNUMTRACKS;
    DoIO(tdreq);

    return tdreq->iotd_Req.io_Actual / NUMHEADS;
}

/*
 * Seek the drive to the indicated cylinder. Use the trackdisk.device for
 * ease. Don't use this when you have allocated the disk via GetDrive().
 */

int
TDSeek(unit, ioreq, cylinder)
UNIT	       *unit;
struct IOStdReq *ioreq;
int		cylinder;
{
    register struct IOExtTD *tdreq = unit->mu_DiskIOReq;

    debug(("TDSeek %ld\n", (long)cylinder));

    tdreq->iotd_Req.io_Command = TD_SEEK;
    tdreq->iotd_Req.io_Offset = cylinder * (TD_SECTOR * NUMSECS * NUMHEADS);
    if ((ioreq->io_Flags & IOMDF_40TRACKS) && (unit->mu_NumCyls == 80))
	tdreq->iotd_Req.io_Offset *= 2;
    DoIO(tdreq);

    return tdreq->iotd_Req.io_Error;
}

void	       *
GetDrive(drunit)
register struct DiskResourceUnit *drunit;
{
    register void  *LastDriver;

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
	    Remove(drunit);
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
    register int    i;
    DEV 	   *dev;
    register UNIT  *unit;

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
register UNIT  *unit;
{
    register struct IOExtTD *tdreq;

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
register UNIT  *unit;
{
    register struct IOExtTD *tdreq;

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
	DoIO(tdreq);
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
register struct IOExtTD *ioreq;
register UNIT  *unit;
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
    userbuf = (byte *) ioreq->iotd_Req.io_Data;
    length = ioreq->iotd_Req.io_Length / MS_BPS;	/* Sector count */
    offset = ioreq->iotd_Req.io_Offset / MS_BPS;	/* Sector number */
    debug(("userbuf %08lx off %ld len %ld ", userbuf, offset, length));

    cylinder = offset / unit->mu_SectorsPerTrack;
    side = cylinder % MS_NSIDES;
    cylinder /= MS_NSIDES;
    sector = offset % unit->mu_SectorsPerTrack;       /* 0..8 or 9 */
    debug(("Tr=%ld Si=%ld Se=%ld\n", (long)cylinder, (long)side, (long)sector));

    ioreq->iotd_Req.io_Actual = 0;

    if (length <= 0 || CheckRequest(ioreq, unit))
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
	    ioreq->iotd_Req.io_Error = unit->mu_SectorStatus[sector];
	    goto end;	    /* Don't use this sector anymore... */
	}
	retrycount = 0;
	CopyMem(diskbuf, userbuf, (long) MS_BPS);
	ioreq->iotd_Req.io_Actual += MS_BPS;
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
		    /* ioreq->iotd_Req.io_Error = IOERR_BADLENGTH; */
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
register struct IOExtTD *ioreq;
UNIT	       *unit;
{
    ioreq->iotd_Req.io_Error = TDERR_NotSpecified;
    TermIO(ioreq);
}

void
TD_Format(ioreq, unit)
register struct IOExtTD *ioreq;
UNIT	       *unit;
{
    ioreq->iotd_Req.io_Error = TDERR_NotSpecified;
    TermIO(ioreq);
}

#endif

void
CMD_Reset(ioreq, unit)
struct IOExtTD *ioreq;
UNIT	       *unit;
{
    unit->mu_CurrentSide = -1;
    unit->mu_TrackChanged = 0;
    TermIO(ioreq);
}

void
CMD_Update(ioreq, unit)
struct IOExtTD *ioreq;
register UNIT  *unit;
{
#ifndef READONLY
    if (unit->mu_TrackChanged && !CheckRequest(ioreq, unit))
	Internal_Update(ioreq, unit);
#endif
    TermIO(ioreq);
}

void
CMD_Clear(ioreq, unit)
struct IOExtTD *ioreq;
UNIT	       *unit;
{
    if (!CheckChanged(ioreq, unit)) {
	unit->mu_CurrentSide = -1;
	unit->mu_TrackChanged = 0;
    }
    TermIO(ioreq);
}

void
TD_Seek(ioreq, unit)
struct IOExtTD *ioreq;
UNIT	       *unit;
{
    if (!CheckChanged(ioreq, unit)) {
	word		cylinder;

	cylinder = (ioreq->iotd_Req.io_Offset / unit->mu_SectorsPerTrack) /
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
struct IOExtTD *ioreq;
UNIT	       *unit;
{
    register struct IOStdReq *req;

    req = &unit->mu_DiskIOReq->iotd_Req;
    req->io_Command = TD_CHANGENUM;
    DoIO(req);

    unit->mu_ChangeNum = req->io_Actual;
    ioreq->iotd_Req.io_Actual = req->io_Actual;
    TermIO(ioreq);
}

int
DevInit(dev)
register DEV   *dev;
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
    register UNIT  *unit;
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
    if (OpenDevice(TD_NAME, UnitNr, tdreq, TDF_ALLOW_NON_3_5)) {
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
	SendIO(dcr);
    }
    NewList(&unit->mu_ChangeIntList);

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
struct IOExtTD *ioreq;
DEV	       *dev;
register UNIT  *unit;
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
	register struct IOExtTD *req = unit->mu_DiskIOReq;

	req->iotd_Req.io_Command = TD_REMCHANGEINT;
	req->iotd_Req.io_Data = (void *) unit->mu_DiskChangeReq;
	DoIO(req);
	WaitIO(unit->mu_DiskChangeReq);
#else
	Disable();
	Remove(unit->mu_DiskChangeReq);
	Enable();
#endif
	DeleteExtIO(unit->mu_DiskChangeReq);
	unit->mu_DiskChangeReq = NULL;
    }
    if (unit->mu_DiskIOReq) {
	if (unit->mu_DiskIOReq->iotd_Req.io_Device)
	    CloseDevice(unit->mu_DiskIOReq);
	DeleteExtIO(unit->mu_DiskIOReq);
	unit->mu_DiskIOReq = NULL;
    }
    FreeMem(unit, (long) sizeof (UNIT));

    return 0;			/* Now unitialized */
}

/*
 * Create missing bindings
 */
/* INDENT OFF */

#asm

lib_vectsize	equ	6
lib_base	equ	-lib_vectsize

_RVOAllocUnit	equ	lib_base-(0*lib_vectsize)
_RVOFreeUnit	equ	lib_base-(1*lib_vectsize)
_RVOGetUnit	equ	lib_base-(2*lib_vectsize)
_RVOGiveUnit	equ	lib_base-(3*lib_vectsize)
_RVOGetUnitID	equ	lib_base-(4*lib_vectsize)

;_AllocUnit:
;		move.l	_DRResource,a6
;		move.l	4(sp),d0
;		jmp	_RVOAllocUnit(a6)
;_FreeUnit:
;		move.l	_DRResource,a6
;		move.l	4(sp),d0
;		jmp	_RVOFreeUnit(a6)
_GetUnit:
		move.l	_DRResource,a6
		move.l	4(sp),a1
		jmp	_RVOGetUnit(a6)
;_GetUnitID:
;		move.l	_DRResource,a6
;		move.l	4(sp),d0
;		jmp	_RVOGetUnitID(a6)
_GiveUnit:
		move.l	_DRResource,a6
		jmp	_RVOGiveUnit(a6)

#endasm
/* INDENT ON */

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
register struct IOStdReq *ioreq;
{
    register UNIT  *unit;

    unit = (UNIT *) ioreq->io_Unit;
    Disable();
    AddTail(&unit->mu_ChangeIntList, ioreq);
    Enable();
    ioreq->io_Flags &= ~IOF_QUICK;	/* So we call ReplyMsg instead of
					 * TermIO */
    /* Note no TermIO */
}

void
TD_Remchangeint(ioreq)
register struct IOStdReq *ioreq;
{
    register struct IOStdReq *intreq;

    intreq = (struct IOStdReq *) ioreq->io_Data;
    Disable();
    Remove(intreq);
    Enable();
    ReplyMsg(&intreq->io_Message);      /* Quick bit always cleared */
    ioreq->io_Error = 0;
    TermIO(ioreq);
}

void
DiskChangeHandler()
{
    auto UNIT	   *unit;
    register struct IOStdReq *ioreq;
    register struct IOStdReq *next;
/* INDENT OFF */
#asm
    movem.l d2-d7/a2-a4,-(sp)
    move.l  a1,-4(a5)        ;unit
#endasm
    /* INDENT ON */
    unit->mu_DiskState = STATEF_UNKNOWN;
    unit->mu_ChangeNum++;
    unit->mu_SectorsPerTrack = MS_SPT;
    for (ioreq = (struct IOStdReq *) unit->mu_ChangeIntList.mlh_Head;
	 next = (struct IOStdReq *) ioreq->io_Message.mn_Node.ln_Succ;
	 ioreq = next) {
	Cause((struct Interrupt *) ioreq->io_Data);
    }
/* INDENT OFF */
#asm
    movem.l (sp)+,d2-d7/a2-a4
#endasm
    /* INDENT ON */
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
register struct IOExtTD *ioreq;
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
    userbuf = (byte *) ioreq->iotd_Req.io_Data;
    length = ioreq->iotd_Req.io_Length / MS_BPS;	/* Sector count */
    offset = ioreq->iotd_Req.io_Offset / MS_BPS;	/* Sector number */
    debug(("userbuf %08lx off %ld len %ld ", userbuf, offset, length));

    spt = unit->mu_SectorsPerTrack;
    cylinder = offset / spt;
    side = cylinder % MS_NSIDES;
    cylinder /= MS_NSIDES;
    sector = offset % spt;
    debug(("T=%ld Si=%ld Se=%ld\n", (long)cylinder, (long)side, (long)sector));

    ioreq->iotd_Req.io_Actual = 0;

    if (length <= 0 || CheckRequest(ioreq, unit))
	goto end;

    GetTrack(ioreq, side, cylinder);
    for (;;) {
	CopyMem(userbuf, unit->mu_TrackBuffer + MS_BPS * sector, (long) MS_BPS);
	unit->mu_TrackChanged = 1;
	unit->mu_SectorStatus[sector] = CRC_CHANGED;

	ioreq->iotd_Req.io_Actual += MS_BPS;
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
	ioreq->iotd_Req.io_Error = TDERR_NotSpecified;

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
struct IOExtTD *ioreq;
register UNIT  *unit;
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
	    register int i;

	    for (i = unit->mu_CurrentSectors - 1; i >= 0; i--) {
		if (unit->mu_SectorStatus[i] == CRC_CHANGED) {
		    unit->mu_CrcBuffer[i] = DataCRC(unit->mu_TrackBuffer + i * MS_BPS);
		    debug(("%ld: %04lx\n", (long)i, (long)unit->mu_CrcBuffer[i]));
		}
	    }
	}
	{
	    DEV 	   *dev;
	    register struct IOExtTD *tdreq;
	    word	    SectorGap;

	    dev = (DEV *) ioreq->iotd_Req.io_Device;
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
		ioreq->iotd_Req.io_Error = TDERR_SeekError;
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
register struct IOExtTD *ioreq;
UNIT	       *unit;
{
    register struct IOExtTD *tdreq = unit->mu_DiskIOReq;
    DEV 	   *dev;
    short	    side;
    int 	    cylinder;
    byte	   *userbuf;
    int 	    length;
    word	    spt;
    word	    gaplen;

    debug(("CMD_Format "));

    if (CheckRequest(ioreq, unit))
	goto termio;

    userbuf = (byte *) ioreq->iotd_Req.io_Data;
    length = ioreq->iotd_Req.io_Length / MS_BPS;	    /* Sector count */
    cylinder = ioreq->iotd_Req.io_Offset / MS_BPS;	    /* Sector number */
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
	ioreq->iotd_Req.io_Error = IOERR_BADLENGTH;
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

    ioreq->iotd_Req.io_Actual = 0;

    /*
     * Write out the current track if we are not going to overwrite it.
     * After the format operation, the buffer is invalidated.
     */
    if (cylinder <= unit->mu_CurrentCylinder &&
	unit->mu_CurrentCylinder < cylinder + length)
	Internal_Update(ioreq, unit);

    dev = (DEV *) ioreq->iotd_Req.io_Device;

    while (length > 0) {
	{
	    register int i;

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
	    ioreq->iotd_Req.io_Error = IOERR_BADLENGTH;
	    break;
	}
	unit->mu_CurrentSide = side;
	unit->mu_CurrentCylinder = cylinder;
	HardwareIO(dev, unit, DSKWRITE, ioreq);

	ReleaseSemaphore(&dev->md_HardwareUse);

	length--;
	userbuf += MS_BPS * spt;
	ioreq->iotd_Req.io_Actual += MS_BPS * spt;

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
/* INDENT OFF */

#asm

; we need a buffer for the Sector-ID field to calculate its checksum
;SectorHeader:
;	 dc.b	 0		   ; cylinder
;	 dc.b	 0		   ; side
;	 dc.b	 0		   ; sector
;	 dc.b	 2		   ; length (2=512 bytes)
;	 dc.w	 0		   ; CRC

	public _EncodeTrack

;   EncodeTrack(TrackBuffer, Rawbuffer, Crcs, Cylinder, Side, GapLen, NumSecs)
;		4	     4		4     2 	2     2       2

_EncodeTrack:
	movem.l d2-d7/a2-a6,-(sp)  ; save registers

fp	set	(4*(6+5))+4        ; 4 for return address
trackbf set	0
rawbf	set	4
crcs	set	8
cylinder   set	   12
side	set	14
gaplen	set	16
numsecs set	18

; a0	ptr in encoded data (also putmfmbyte)
; a2	ptr to mfm encoding table (putmfmbyte)
; a3	ptr to data to be crc'd (HeaderCRC)
; a4	ptr to table with calculated CRC's
; a5	ptr to unencoded data

; d0	byte to be encoded (putmfmbyte)
; d1	trashed by putmfmbyte
; d3	used by putmfmbyte
; d5	sector number
; d6	general counter of byte spans
; d7	sector countdown

	sub.w	#2,fp+gaplen(sp)   ; gap length between sectors
	move.l	fp+rawbf(sp),a0    ; pointer to mfmencoded buffer
	move.l	fp+crcs(sp),a4     ; pointer to precalculated CRCs
	move.l	fp+trackbf(sp),a5  ; pointer to unencoded data
	lea	_MfmEncode,a2	   ; pointer to MFM lookup table

	move.w	#$9254,d0	   ; a track starts with a gap ($4e)
	moveq	#INDEXGAP-1,d6
ingl1	move.w	d0,(a0)+           ; mfmencoded = $9254
	dbf	d6,ingl1

	move.w	#$aaaa,d0	   ; a track index starts with a gap containing
	moveq	#INDXGAP-1,d6	   ; 12 * 0 (mfm = $aaaa)
ingl2	move.w	d0,(a0)+
	dbf	d6,ingl2

	move.w	#INDEXSYNC,d0	   ; The INDEX field begins here, starting
	move.w	d0,(a0)+           ; with 3 syncs (3 * $c2) with a missing
	move.w	d0,(a0)+           ; clock bit
	move.w	d0,(a0)+

	move.w	#$5552,(a0)+       ; INDEX mark ($fc)

	move.w	#$9254,d0	   ; more gap
	moveq	#INDEXGAP2-1,d6
ingl3	move.w	d0,(a0)+           ; mfmencoded = $9254
	dbf	d6,ingl3

	lea	-6(sp),sp          ; Reserve room for SectorHeader
fp	set	fp+6
	move.w	fp+numsecs(sp),d7  ; number of sectors to encode
	subq.w	#1,d7		   ; minus 1 for dbra
	moveq	#0,d5		   ; start with first sector

secloop:
	move.w	#$aaaa,d0	   ; a sector starts with a gap containing
	moveq	#IDGAP2-1,d6	   ; 12 * 0 (mfm = $aaaa)
id2gl	move.w	d0,(a0)+
	dbf	d6,id2gl

	move.w	#SYNC,d0	   ; The ID field begins here, starting
	move.w	d0,(a0)+           ; with 3 syncs (3 * $a1) with a missing
	move.w	d0,(a0)+           ; clock bit
	move.w	d0,(a0)+

	move.w	#$5554,(a0)+       ; ID-Address mark ($fe)

	move.l	sp,a3		   ; pointer to Sector-ID buffer

	moveq	#$5554&1,d3	   ; preload d3 for the putmfmbyte routine
	move.b	fp+cylinder+1(sp),0(a3)  ; insert current cylinder number
	move.b	fp+side+1(sp),1(a3)   ; side number
	addq.w	#1,d5		   ; sectors start with 1 instead of 0
	move.b	d5,2(a3)           ; sector number
	move.b	#MS_BPScode,3(a3)  ; sector length 512 bytes
	bsr	HeaderCRC	   ; calculate checksum
	move.w	d0,IDDATA(a3)      ; put it past the data

	moveq	#IDDATA+IDCRC-1,d6 ; 6 bytes Sector-ID
sidl	move.b	(a3)+,d0           ; get one byte
	bsr	putmfmbyte	   ; encode it
	dbf	d6,sidl 	   ; end of buffer ?

	moveq	#$4e,d0 	   ; recalculate the MFM value of the
	bsr	putmfmbyte	   ; first gap byte

	moveq	#DATAGAP1-1-1,d6   ; GAP consisting of
	move.w	#$9254,d0	   ; 22 * $4e
dg1l	move.w	d0,(a0)+
	dbf	d6,dg1l

	moveq	#DATAGAP2-1,d6	   ; GAP consisting of
	move.w	#$aaaa,d0	   ; 12 * 0 (mfm = $aaaa)
dg2l	move.w	d0,(a0)+
	dbf	d6,dg2l

	move.w	#SYNC,d0	   ; Sector data
	move.w	d0,(a0)+           ; starts with 3 syncs
	move.w	d0,(a0)+
	move.w	d0,(a0)+

	move.w	#$5545,(a0)+       ; Data Address Mark ($fb)

	moveq	#$5545&1,d3	   ; preload d3
	move	#MS_BPS-1,d6	   ; a sector has 512 bytes
dblockl move.b	(a5)+,d0           ; get one byte from the buffer
	bsr	putmfmbyte	   ; encode it
	dbf	d6,dblockl	   ; end of sector ?

	move.b	(a4)+,d0           ; get first byte of CRC
	bsr	putmfmbyte	   ; encode it
	move.b	(a4)+,d0           ; get second byte
	bsr	putmfmbyte	   ; encode it

	moveq	#$4e,d0 	   ; recalculate the MFM value of the
	bsr	putmfmbyte	   ; first gap byte -> -1 in following loop

;	moveq	#DATAGAP3-1-1,d6   ; sector ends with a gap
	move.w	fp+gaplen(sp),d6   ; sector ends with a gap, -1 for dbf
	move.w	#$9254,d0	   ; 80 * $4e
dg3l	move.w	d0,(a0)+
	dbf	d6,dg3l

	dbf	d7,secloop	   ; next sector. d5 has been incremented

	lea	6(sp),sp           ; Free room for SectorHeader
fp	set	fp-6

	move.l	fp+rawbf(sp),d6    ; pointer to mfmencoded buffer
	add.l	#WLEN-2,d6	   ; end of encoded buffer (-2 for dbf)
	move.l	a0,d0		   ; (I really want to   sub.l a0,d6 )
	sub.l	d0,d6		   ; length of the remains
	lsr.l	#1,d6		   ; turn into words

	move.w	#$9254,d0	   ; Fill the end of the track with $4e
endgl	move.w	d0,(a0)+           ; $9254 mfm encoded
	dbf	d6,endgl

	movem.l (sp)+,d2-d7/a2-a6
	rts

; putmfmbyte encodes one byte (in D0) into MSDOS MFM format to the location
; pointed by A0. D3 has to be preserved between calls !
; A2 must contain the pointer to the encoding table.
; Destroys D0, D1. Updates A0 and D3. Requires A0, D0, D3.

putmfmbyte
	moveq	#16-4,d1
	lsl.l	d1,d0		; split the byte into two nibbles
	lsr.w	d1,d0		; low nibble is in bits 0..15
				; high nibble in bits 16..31
	swap	d0		; process high nibble first
	and.w	#$0f,d0 	; mask out unwanted bits
	move.b	0(a2,d0.w),d1   ; get mfmencoded nibble from table
	btst	#6,d1		; we now have to work out if
	bne.s	1$		; the high bit of the unencoded data
	btst	#0,d3		; byte and the low bit of the last
	bne.s	1$		; encoded data are both 0. if this is the
	bset	#7,d1		; case the first clock bit has to be '1'
1$	move.b	d1,(a0)+        ; write high (encoded) nibble
	swap	d0		; process low nibble
	move.b	0(a2,d0.w),d3   ; ....same as above
	btst	#6,d3
	bne.s	2$
	btst	#0,d1
	bne.s	2$
	bset	#7,d3
2$	move.b	d3,(a0)+
	rts

#endasm
#endif				/* READONLY */
#asm

; The CRC is computed not only over the actual data, but including
; the SYNC mark (3 * $a1) and the 'ID/DATA - Address Mark' ($fe/$fb).
; As we don't read or encode these fields into our buffers, we have to
; preload the registers containing the CRC with the values they would have
; after stepping over these fields.
;
; How CRCs "really" work:
;
; First, you should regard a bitstring as a series of coefficients of
; polymomials. We calculate with these polynomials in modulo-2
; arithmetic, in which both add and subtract are done the same as
; exclusive-or. Now, we modify our data (a very long polynomial) in
; such a way that it becomes divisible by the CCITT-standard 16-bit
;		 16   12   5
; polynomial:	x  + x	+ x + 1, represented by $11021. The easiest
; way to do this would be to multiply (using proper arithmetic) our
; datablock with $11021. So we have:
;   data * $11021		 =
;   data * ($10000 + $1021)      =
;   data * $10000 + data * $1021
; The left part of this is simple: Just add two 0 bytes. But then
; the right part (data $1021) remains difficult and even could have
; a carry into the left part. The solution is to use a modified
; multiplication, which has a result that is not correct, but with
; a difference of any multiple of $11021. We then only need to keep
; the 16 least significant bits of the result.
;
; The following algorithm does this for us:
;
;   unsigned char *data, c, crclo, crchi;
;   while (not done) {
;	c = *data++ + crchi;
;	crchi = (@ c) >> 8 + crclo;
;	crclo = @ c;
;   }
;
; Remember, + is done with EOR, the @ operator is in two tables (high
; and low byte separately), which is calculated as
;
;      $1021 * (c & $F0)
;  xor $1021 * (c & $0F)
;  xor $1021 * (c >> 4)         (* is regular multiplication)
;
;
; Anyway, the end result is the same as the remainder of the division of
; the data by $11021. I am afraid I need to study theory a bit more...


; This is the entry to calculate the checksum for the sector-id field
; requires:  a3 = pointer to the unencoded data
; returns:   d0 = CRC

HeaderCRC:
	movem.l  d1-d3/a3-a5,-(sp) ; save registers
	move.w	 #$b2,d0	   ; preload registers
	moveq	 #$30,d1	   ; (CRC for $a1,$a1,$a1,$fb)
	moveq	 #3,d3		   ; calculate checksum for 4 bytes
	bra.s	 getCRC 	   ; (=cylinder,side,sector,sectorlength)

; This is the entry to calculate the checksum for the data field
; requires:  a3 = pointer to the unencoded data
; returns:   d0 = CRC

; C entry point for DataCRC(byte *data)

_DataCRC:
	movem.l d1-d3/a3-a5,-(sp)  ; save registers
fp	set	(4*(3+3))+4
data	set	0
	move.l	fp+data(sp),a3     ; get parameter
	move.w	 #$e2,d0	   ; preload the CRC registers
	move.w	 #$95,d1	   ; (CRC for $a1,$a1,$a1,$fe)
	move.w	 #MS_BPS-1,d3	   ; a sector 512 bytes

getCRC	lea	 CRCTable1,a4
	lea	 CRCTable2,a5
	moveq	 #0,d2

1$	move.b	 (a3)+,d2
	eor.b	 d0,d2
	move.b	 0(a4,d2.w),d0
	eor.b	 d1,d0
	move.b	 0(a5,d2.w),d1
	dbf	 d3,1$

	lsl.w	 #8,d0
	move.b	 d1,d0
	movem.l  (sp)+,d1-d3/a3-a5
	rts


CRCTable1:
	dc.b $00,$10,$20,$30,$40,$50,$60,$70,$81,$91,$a1,$b1,$c1,$d1,$e1,$f1
	dc.b $12,$02,$32,$22,$52,$42,$72,$62,$93,$83,$b3,$a3,$d3,$c3,$f3,$e3
	dc.b $24,$34,$04,$14,$64,$74,$44,$54,$a5,$b5,$85,$95,$e5,$f5,$c5,$d5
	dc.b $36,$26,$16,$06,$76,$66,$56,$46,$b7,$a7,$97,$87,$f7,$e7,$d7,$c7
	dc.b $48,$58,$68,$78,$08,$18,$28,$38,$c9,$d9,$e9,$f9,$89,$99,$a9,$b9
	dc.b $5a,$4a,$7a,$6a,$1a,$0a,$3a,$2a,$db,$cb,$fb,$eb,$9b,$8b,$bb,$ab
	dc.b $6c,$7c,$4c,$5c,$2c,$3c,$0c,$1c,$ed,$fd,$cd,$dd,$ad,$bd,$8d,$9d
	dc.b $7e,$6e,$5e,$4e,$3e,$2e,$1e,$0e,$ff,$ef,$df,$cf,$bf,$af,$9f,$8f
	dc.b $91,$81,$b1,$a1,$d1,$c1,$f1,$e1,$10,$00,$30,$20,$50,$40,$70,$60
	dc.b $83,$93,$a3,$b3,$c3,$d3,$e3,$f3,$02,$12,$22,$32,$42,$52,$62,$72
	dc.b $b5,$a5,$95,$85,$f5,$e5,$d5,$c5,$34,$24,$14,$04,$74,$64,$54,$44
	dc.b $a7,$b7,$87,$97,$e7,$f7,$c7,$d7,$26,$36,$06,$16,$66,$76,$46,$56
	dc.b $d9,$c9,$f9,$e9,$99,$89,$b9,$a9,$58,$48,$78,$68,$18,$08,$38,$28
	dc.b $cb,$db,$eb,$fb,$8b,$9b,$ab,$bb,$4a,$5a,$6a,$7a,$0a,$1a,$2a,$3a
	dc.b $fd,$ed,$dd,$cd,$bd,$ad,$9d,$8d,$7c,$6c,$5c,$4c,$3c,$2c,$1c,$0c
	dc.b $ef,$ff,$cf,$df,$af,$bf,$8f,$9f,$6e,$7e,$4e,$5e,$2e,$3e,$0e,$1e

CRCTable2:
	dc.b $00,$21,$42,$63,$84,$a5,$c6,$e7,$08,$29,$4a,$6b,$8c,$ad,$ce,$ef
	dc.b $31,$10,$73,$52,$b5,$94,$f7,$d6,$39,$18,$7b,$5a,$bd,$9c,$ff,$de
	dc.b $62,$43,$20,$01,$e6,$c7,$a4,$85,$6a,$4b,$28,$09,$ee,$cf,$ac,$8d
	dc.b $53,$72,$11,$30,$d7,$f6,$95,$b4,$5b,$7a,$19,$38,$df,$fe,$9d,$bc
	dc.b $c4,$e5,$86,$a7,$40,$61,$02,$23,$cc,$ed,$8e,$af,$48,$69,$0a,$2b
	dc.b $f5,$d4,$b7,$96,$71,$50,$33,$12,$fd,$dc,$bf,$9e,$79,$58,$3b,$1a
	dc.b $a6,$87,$e4,$c5,$22,$03,$60,$41,$ae,$8f,$ec,$cd,$2a,$0b,$68,$49
	dc.b $97,$b6,$d5,$f4,$13,$32,$51,$70,$9f,$be,$dd,$fc,$1b,$3a,$59,$78
	dc.b $88,$a9,$ca,$eb,$0c,$2d,$4e,$6f,$80,$a1,$c2,$e3,$04,$25,$46,$67
	dc.b $b9,$98,$fb,$da,$3d,$1c,$7f,$5e,$b1,$90,$f3,$d2,$35,$14,$77,$56
	dc.b $ea,$cb,$a8,$89,$6e,$4f,$2c,$0d,$e2,$c3,$a0,$81,$66,$47,$24,$05
	dc.b $db,$fa,$99,$b8,$5f,$7e,$1d,$3c,$d3,$f2,$91,$b0,$57,$76,$15,$34
	dc.b $4c,$6d,$0e,$2f,$c8,$e9,$8a,$ab,$44,$65,$06,$27,$c0,$e1,$82,$a3
	dc.b $7d,$5c,$3f,$1e,$f9,$d8,$bb,$9a,$75,$54,$37,$16,$f1,$d0,$b3,$92
	dc.b $2e,$0f,$6c,$4d,$aa,$8b,$e8,$c9,$26,$07,$64,$45,$a2,$83,$e0,$c1
	dc.b $1f,$3e,$5d,$7c,$9b,$ba,$d9,$f8,$17,$36,$55,$74,$93,$b2,$d1,$f0

#endasm

/* INDENT ON */
