/*-
 * $Id$
 * $Log$
 *
 *  HANSEC.C
 *
 *  The code for the messydos file system handler
 *
 *  Sector-level stuff: read, write, cache, unit conversion.
 *  Other interactions (via MyDoIO) with messydisk.device.
 *
 *  This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 *  not be used or copied without a licence.
-*/

#include "han.h"
#include "dos.h"

#ifdef DEBUG
#   define	debug(x)  dbprintf x
#else
#   define	debug(x)
#endif

struct MsgPort *DiskReplyPort;
struct IOExtTD *DiskIOReq;
struct IOStdReq *DiskChangeReq;

struct DiskParam Disk;
byte	       *Fat;
short		FatDirty;	/* Fat must be written to disk */

short		error;		/* To put the error value; for Result2 */
long		IDDiskState;	/* InfoData.id_DiskState */
long		IDDiskType;	/* InfoData.id_DiskType */
struct timerequest *TimeIOReq;	/* For motor-off delay */
struct MinList	CacheList;	/* Sector cache */
int		CurrentCache;	/* How many cached buffers do we have */
int		MaxCache = 5;	/* Maximum amount of cached buffers */
long		CacheBlockSize; /* Size of disk block + overhead */
ulong		BufMemType;
int		DelayState;


byte	       *Word8086;

word
Get8086Word(offset)
register int	offset;
{
    return Word8086[offset] | Word8086[offset + 1] << 8;
}

word
OtherEndianWord(oew)
word		oew;
{
/* INDENT OFF */
#asm
	move.w	8(a5),d0
	rol.w	#8,d0
#endasm
    /* INDENT ON */
    /*
     * return (oew << 8) | ((oew >> 8) & 0xff);
     */
}

ulong
OtherEndianLong(oel)
ulong		oel;
{
/* INDENT OFF */
#asm
	move.l	8(a5),d0
	rol.w	#8,d0
	swap	d0
	rol.w	#8,d0
#endasm
    /* INDENT ON */
    /*
     * return ((oel &       0xff) << 24) | ((oel &     0xff00) <<  8) |
     * ((oel &   0xff0000) >>  8) | ((oel & 0xff000000) >> 24);
     */
}

void
OtherEndianMsd(msd)
register struct MsDirEntry *msd;
{
    msd->msd_Date = OtherEndianWord(msd->msd_Date);
    msd->msd_Time = OtherEndianWord(msd->msd_Time);
    msd->msd_Cluster = OtherEndianWord(msd->msd_Cluster);
    msd->msd_Filesize = OtherEndianLong(msd->msd_Filesize);
}

word
ClusterToSector(cluster)
register word	cluster;
{
    return cluster ? Disk.start + cluster * Disk.spc
	: 0;
}

word
ClusterOffsetToSector(cluster, offset)
register word	cluster;
register word	offset;
{
    return cluster ? Disk.start + cluster * Disk.spc + offset / Disk.bps
	: 0;
}

word
DirClusterToSector(cluster)
register word	cluster;
{
    return cluster ? Disk.start + cluster * Disk.spc
	: Disk.rootdir;
}


word
SectorToCluster(sector)
register word	sector;
{
    return sector ? (sector - Disk.start) / Disk.spc
	: 0;
}

/*
 * Get the next cluster in a chain. Sort-of checks for special entries.
 */

word
NextCluster(cluster)
word cluster;
{
    register word entry;


    return (entry = GetFatEntry(cluster)) >= 0xFF7 ? FAT_EOF : entry;
}

word
NextClusteredSector(sector)
word		sector;
{
    word	    next = (sector + 1 - Disk.start) % Disk.spc;

    if (next == 0) {
	next = NextCluster(SectorToCluster(sector));
	return next != FAT_EOF ? ClusterToSector(next)
	    : SEC_EOF;
    } else
	return sector + 1;
}

#ifndef READONLY

word
FindFreeSector(prev)
word		prev;
{
    word freecluster = FindFreeCluster(SectorToCluster(prev));

    return freecluster == FAT_EOF ? SEC_EOF : ClusterToSector(freecluster);
}

#endif

/*
 * Find a specific sector. The cache list is a Least Recently Used stack:
 * Put it on the head of the cache list. So if it is not used anymore in a
 * long time, it bubbles to the end of the list, getting a higher chance
 * of being trashed for re-use.
 */

struct CacheSec *
FindSecByNumber(number)
register int	number;
{
    register struct CacheSec *sec;
    register

    debug(("FindSec %d", number));

    for (sec = (void *) CacheList.mlh_Head;
	 nextsec = (void *) sec->sec_Node.mln_Succ; sec = nextsec) {
	if (sec->sec_Number == number) {
	    debug((" (%x) %lx\n", sec->sec_Refcount, sec));
	    Remove(sec);
	    AddHead(&CacheList, &sec->sec_Node);
	    return sec;
	}
    }

    debug(("; "));
    return NULL;
}

struct CacheSec *
FindSecByBuffer(buffer)
byte	       *buffer;
{
    return (struct CacheSec *) (buffer - OFFSETOF(CacheSec, sec_Data));
}

/*
 * Get a fresh cache buffer. If we are allowed more cache, we just
 * allocate memory. Otherwise, we try to find a currently unused buffer.
 * We start looking at the end of the list, which is the bottom of the LRU
 * stack. If that fails, allocate more memory anyway. Not that is likely
 * anyway, since we currently lock only one sector at a time.
 */

struct CacheSec *
NewCacheSector()
{
    register struct CacheSec *sec;
    register struct CacheSec *nextsec;

    debug(("NewCacheSector\n"));

    if (CurrentCache < MaxCache) {
	if (sec = AllocMem(CacheBlockSize, BufMemType)) {
	    goto add;
	}
    }
    for (sec = (void *) CacheList.mlh_TailPred;
	 nextsec = (void *) sec->sec_Node.mln_Pred; sec = nextsec) {
	if ((CurrentCache > MaxCache) && (sec->sec_Refcount == SEC_DIRTY)) {
	    FreeCacheSector(sec);       /* Also writes it to disk */
	    continue;
	}
	if (sec->sec_Refcount == 0)     /* Implies not SEC_DIRTY */
	    return sec;
    }

    sec = AllocMem(CacheBlockSize, BufMemType);

    if (sec) {
add:
	CurrentCache++;
	AddHead(&CacheList, &sec->sec_Node);
    } else
	error = ERROR_NO_FREE_STORE;

    return sec;
}

/*
 * Dispose a cached sector, even if it has a non-zero refcount. If it is
 * dirty, write it out.
 */

void
FreeCacheSector(sec)
register struct CacheSec *sec;
{
    Remove(sec);
#ifndef READONLY
    if (sec->sec_Refcount & SEC_DIRTY) {
	PutSec(sec->sec_Number, sec->sec_Data);
    }
#endif
    FreeMem(sec, CacheBlockSize);
    CurrentCache--;
}

/*
 * Create an empty cache list
 */

void
InitCacheList()
{
    extern struct CacheSec *sec;    /* Of course this does not exist... */

    NewList(&CacheList);
    CurrentCache = 0;
    CacheBlockSize = Disk.bps + sizeof (*sec) - sizeof (sec->sec_Data);
}

/*
 * Dispose all cached sectors, possibly writing them to disk.
 */

void
FreeCacheList()
{
    register struct CacheSec *sec;

    debug(("FreeCacheList, %d\n", CurrentCache));
    while (sec = GetHead(&CacheList)) {
	FreeCacheSector(sec);
    }
}

/*
 * Write all dirty cache buffers to disk. It would be nice to sort them,
 * but currently we don't do that yet.
 */

void
MSUpdate(immediate)
int		immediate;
{
    register struct CacheSec *sec;
    register struct CacheSec *nextsec;

    debug(("MSUpdate\n"));

#ifndef READONLY
    if (FatDirty) {
	WriteFat();
    }
    if (DelayState & DELAY_DIRTY) {
	for (sec = (void *) CacheList.mlh_TailPred;
	     nextsec = (void *) sec->sec_Node.mln_Pred; sec = nextsec) {
	    if (sec->sec_Refcount & SEC_DIRTY) {
		PutSec(sec->sec_Number, sec->sec_Data);
		sec->sec_Refcount &= ~SEC_DIRTY;
	    }
	}
	DelayState &= ~DELAY_DIRTY;
    }
#endif

    if (immediate)
	DelayState = DELAY_RUNNING1;

    if (DelayState & DELAY_RUNNING2) {
	StartTimer();
	DelayState &= ~DELAY_RUNNING2;
    } else {			/* DELAY_RUNNING1 */
#ifndef READONLY
	while (TDUpdate() != 0 && RetryRwError())
	    ;
#endif
	TDMotorOff();
	DelayState = DELAY_OFF;
    }
}

/*
 * Start the timer which triggers cache writing and stopping the disk
 * motor.
 */

void
StartTimer()
{
    DelayState |= DELAY_RUNNING1 | DELAY_RUNNING2;

    if (CheckIO(TimeIOReq)) {
	WaitIO(TimeIOReq);
	TimeIOReq->tr_node.io_Command = TR_ADDREQUEST;
	TimeIOReq->tr_time.tv_secs = 3;
	TimeIOReq->tr_time.tv_micro = 0;
	SendIO(TimeIOReq);
    }
}

/*
 * Get a pointer to a logical sector { 0, ..., MS_SECTCNT - 1}. We
 * allocate a buffer and copy the data in, and lock the buffer until
 * FreeSec() is called.
 */

byte	       *
GetSec(sector)
int		sector;
{
    struct CacheSec *sec;

    if (sec = FindSecByNumber(sector)) {
	sec->sec_Refcount++;

	return sec->sec_Data;
    }
    if (sec = NewCacheSector()) {
	register struct IOExtTD *req;

	sec->sec_Number = sector;
	sec->sec_Refcount = 1;

	debug(("GetSec %d\n", sector));

	req = DiskIOReq;
	do {
	    req->iotd_Req.io_Command = ETD_READ;
	    req->iotd_Req.io_Data = (APTR)sec->sec_Data;
	    req->iotd_Req.io_Offset = Disk.lowcyl + (long) sector * Disk.bps;
	    req->iotd_Req.io_Length = Disk.bps;
	    MyDoIO(req);
	} while (req->iotd_Req.io_Error != 0 && RetryRwError(req));

	StartTimer();

	if (req->iotd_Req.io_Error == 0) {
	    return sec->sec_Data;
	}
	error = ERROR_NOT_A_DOS_DISK;
	FreeCacheSector(sec);
    }
    return NULL;
}

#ifndef READONLY

byte	       *
EmptySec(sector)
int		sector;
{
    byte	   *buffer;
    register struct CacheSec *sec;

    if (sec = FindSecByNumber(sector)) {
	sec->sec_Refcount++;

	return sec->sec_Data;
    }
    if (sec = NewCacheSector()) {
	sec->sec_Number = sector;
	sec->sec_Refcount = 1;

	return sec->sec_Data;
    }

    return NULL;
}

void
PutSec(sector, data)
int		sector;
byte	       *data;
{
    register struct IOExtTD *req;

    debug(("PutSec %d\n", sector));

    req = DiskIOReq;
    do {
	req->iotd_Req.io_Command = ETD_WRITE;
	req->iotd_Req.io_Data = (APTR) data;
	req->iotd_Req.io_Offset = Disk.lowcyl + (long) sector * Disk.bps;
	req->iotd_Req.io_Length = Disk.bps;
	MyDoIO(req);
    } while (req->iotd_Req.io_Error != 0 && RetryRwError(req));

    StartTimer();
}

#endif

/*
 * Unlock a cached sector. When the usage count drops to zero, which
 * implies it is not dirty, and we are over our cache quota, the sector is
 * freed. Otherwise we keep it for re-use.
 */

void
FreeSec(buffer)
byte	       *buffer;
{
    register struct CacheSec *sec;

    if (sec = FindSecByBuffer(buffer)) {
	if (--sec->sec_Refcount == 0) { /* Implies not SEC_DIRTY */
	    if (CurrentCache > MaxCache) {
		FreeCacheSector(sec);
	    }
	}
    }
}

#ifndef READONLY

void
MarkSecDirty(buffer)
byte	       *buffer;
{
    register struct CacheSec *sec;

    if (sec = FindSecByBuffer(buffer)) {
	sec->sec_Refcount |= SEC_DIRTY;
	DelayState |= DELAY_DIRTY;
	StartTimer();
    }
}

/*
 * Write out the FAT. Called from MSUpdate(), so don't call it again from
 * here. Don't use precious cache space for it; you could say it has its
 * own private cache already.
 */

void
WriteFat()
{
    register int    fat,
		    sec;
    int 	    disksec = Disk.res;      /* First FAT, first sector */

    /* Write all FATs */
    for (fat = 0; fat < Disk.nfats; fat++) {
	for (sec = 0; sec < Disk.spf; sec++) {
	    PutSec(disksec++, Fat + sec * Disk.bps);
	    /* return;	       /* Fat STILL dirty! */
	}
    }
    FatDirty = FALSE;
}

#endif

int
ReadBootBlock()
{
    int protstatus;

    debug(("ReadBootBlock\n"));
    FreeFat();                  /* before disk parameters change */
    TDClear();

    if ((protstatus = TDProtStatus()) >= 0) {
	TDChangeNum();
	debug(("Changenumber = %ld\n", DiskIOReq->iotd_Count));
	if (Word8086 = GetSec(0)) {
	    word bps;

	    /* 8086 ml for a jump */
	    if (Word8086[0] != 0xE9 && Word8086[0] != 0xEB) {
		goto nodisk;
	    }
	    bps = Get8086Word(0x0b);
	    Disk.spc = Word8086[0x0d];
	    Disk.res = Get8086Word(0x0e);
	    Disk.nfats = Word8086[0x10];
	    Disk.ndirs = Get8086Word(0x11);
	    Disk.nsects = Get8086Word(0x13);
	    Disk.media = Word8086[0x15];
	    Disk.spf = Get8086Word(0x16);
	    Disk.spt = Get8086Word(0x18);
	    Disk.nsides = Get8086Word(0x1a);
	    Disk.nhid = Get8086Word(0x1c);
	    FreeSec(Word8086);

	    /*
	     *	Maybe the sector size just changed. Who knows?
	     */
	    if (Disk.bps != bps) {
		FreeCacheList();
		Disk.bps = bps;
		InitCacheList();
	    }

	    Disk.ndirsects = (Disk.ndirs * MS_DIRENTSIZE) / Disk.bps;
	    Disk.rootdir = Disk.res + Disk.spf * Disk.nfats;
	    Disk.datablock = Disk.rootdir + Disk.ndirsects;
	    Disk.start = Disk.datablock - MS_FIRSTCLUST * Disk.spc;
	    /* Available clusters are 2..maxclust in secs start..nsects-1 */
	    Disk.maxclst = (Disk.nsects - Disk.start) / Disk.spc - 1;
	    Disk.bpc = Disk.bps * Disk.spc;
	    Disk.vollabel = FakeRootDirEntry;

	    debug(("%x\tbytes per sector\n", Disk.bps));
	    debug(("%x\tsectors per cluster\n", Disk.spc));
	    debug(("%x\treserved blocks\n", Disk.res));
	    debug(("%x\tfats\n", Disk.nfats));
	    debug(("%x\tdirectory entries\n", Disk.ndirs));
	    debug(("%x\tsectors\n", Disk.nsects));
	    debug(("%x\tmedia byte\n", Disk.media));
	    debug(("%x\tsectors per FAT\n", Disk.spf));
	    debug(("%x\tsectors per track\n", Disk.spt));
	    debug(("%x\tsides\n", Disk.nsides));
	    debug(("%x\thidden sectors\n", Disk.nhid));

	    debug(("%x\tdirectory sectors\n", Disk.ndirsects));
	    debug(("%x\troot dir block\n", Disk.rootdir));
	    debug(("%x\tblock for (imaginary) cluster 0\n", Disk.start));
	    debug(("%x\tfirst data block\n", Disk.datablock));
	    debug(("%x\tclusters total\n", Disk.maxclst));
	    debug(("%x\tbytes per cluster\n", Disk.bpc));

	    IDDiskType = ID_DOS_DISK;
#ifdef READONLY
	    IDDiskState = ID_WRITE_PROTECTED;
#else
	    if (protstatus > 0)
		IDDiskState = ID_WRITE_PROTECTED;
	    else
		IDDiskState = ID_VALIDATED;
#endif

	    if (Disk.nsects / (MS_SPT * Disk.nsides) <= 40)
		DiskIOReq->iotd_Req.io_Flags |= IOMDF_40TRACKS;
	    else
		DiskIOReq->iotd_Req.io_Flags &= ~IOMDF_40TRACKS;

	    GetFat();
	} else {
	    debug(("Can't read %d.\n", DiskIOReq->iotd_Req.io_Error));
	nodisk:
	    FreeCacheList();
	    error = ERROR_NO_DISK;
	    IDDiskType = ID_UNREADABLE_DISK;
	    IDDiskState = ID_WRITE_PROTECTED;
	}
    }
#ifdef DEBUG
    else debug(("No disk inserted %d.\n", DiskIOReq->iotd_Req.io_Error));
#endif
    return 1;
}

/*
 * We try to identify the disk currently in the drive, trying to find the
 * volume label in the first directory block.
 */

int
IdentifyDisk(name, date)
char	       *name;		/* Should be at least 32 characters */
struct DateStamp *date;
{
    debug(("IdentifyDisk\n"));
    ReadBootBlock();            /* Also sets default vollabel */

    if (IDDiskType == ID_DOS_DISK) {
	byte	       *dirblock;
	register struct MsDirEntry *dirent;

	if (dirblock = GetSec(Disk.rootdir)) {
	    dirent = (struct MsDirEntry *) dirblock;

	    while ((byte *) dirent < &dirblock[Disk.bps]) {
		if (dirent->msd_Attributes & ATTR_VOLUMELABEL) {
		    Disk.vollabel.de_Msd = *dirent;
		    Disk.vollabel.de_Sector = Disk.rootdir;
		    Disk.vollabel.de_Offset = (byte *) dirent - dirblock;
		    OtherEndianMsd(&Disk.vollabel.de_Msd);
		    Disk.vollabel.de_Msd.msd_Cluster = 0;	/* to be sure */
/*		    Disk.vollabel.de_Msd.msd_Attributes |= ATTR_DIRECTORY; */
		    break;
		}
		dirent++;
	    }
	    strncpy(name, Disk.vollabel.de_Msd.msd_Name, 8 + 3);
	    name[8 + 3] = '\0';
	    ZapSpaces(name, name + 8 + 3);
	    ToDateStamp(date, Disk.vollabel.de_Msd.msd_Date,
			Disk.vollabel.de_Msd.msd_Time);
	    debug(("Disk is called '%s'\n", name));

	    FreeSec(dirblock);

	    return 0;
	}
    }
    return 1;
}

/*
 * Remove the disk change SoftInt. The V1.2 / V1.3 version is broken, so
 * we use a workaround. The correct thing to do is shown but not used.
 */

void
TDRemChangeInt()
{
    if (DiskChangeReq) {
	register struct IOExtTD *req = DiskIOReq;

#if 0				/* V1.2 and V1.3 have a broken
				 * TD_REMCHANGEINT */
	req->iotd_Req.io_Command = TD_REMCHANGEINT;
	req->iotd_Req.io_Data = (void *) DiskChangeReq;
	MyDoIO(req);
	WaitIO(DiskChangeReq);
#else
	Forbid();
	Remove(DiskChangeReq);
	Permit();
#endif
	DeleteExtIO(DiskChangeReq);
	DiskChangeReq = NULL;
    }
}

/*
 * Set the disk change SoftInt. Return nonzero on failure.
 */

int
TDAddChangeInt(interrupt)
struct Interrupt *interrupt;
{
    register struct IOExtTD *req = DiskIOReq;

    if (DiskChangeReq) {
	TDRemChangeInt();
    }
    DiskChangeReq = (void *)CreateExtIO(DiskReplyPort,
					 (long) sizeof (*DiskChangeReq));
    if (DiskChangeReq) {
	/* Clone IO request part */
	DiskChangeReq->io_Device = req->iotd_Req.io_Device;
	DiskChangeReq->io_Unit = req->iotd_Req.io_Unit;
	DiskChangeReq->io_Command = TD_ADDCHANGEINT;
	DiskChangeReq->io_Data = (void *) interrupt;
	SendIO(DiskChangeReq);

	return 0;
    }
    return 1;
}

/*
 * Get the current disk change number. Necessary for ETD_ commands. Makes
 * absolutely sure nobody can change the disk without us noticing it.
 */

int
TDChangeNum()
{
    register struct IOExtTD *req = DiskIOReq;

    req->iotd_Req.io_Command = TD_CHANGENUM;
    MyDoIO(req);
    req->iotd_Count = req->iotd_Req.io_Actual;

    return req->iotd_Req.io_Actual;
}

/*
 * Get the current write protection state.
 *
 * Zero means writable, one means write protected, minus one means
 * no disk in drive.
 */

int
TDProtStatus()
{
    register struct IOExtTD *req = DiskIOReq;

    req->iotd_Req.io_Command = TD_PROTSTATUS;
    MyDoIO(req);

    if (req->iotd_Req.io_Error)
	return -1;

    return req->iotd_Req.io_Actual != 0;
}

/*
 * Switch the drive motor off. Return previous state. Don't use this when
 * you have allocated the disk via GetDrive().
 */

int
TDMotorOff()
{
    register struct IOExtTD *req = DiskIOReq;

    req->iotd_Req.io_Command = TD_MOTOR;
    req->iotd_Req.io_Length = 0;
    MyDoIO(req);

    return req->iotd_Req.io_Actual;
}

/*
 * Clear all internal messydisk buffers.
 */

int
TDClear()
{
    register struct IOExtTD *req = DiskIOReq;

    req->iotd_Req.io_Command = CMD_CLEAR;

    return MyDoIO(req);
}

#ifndef READONLY
/*
 * Write out all internal messydisk buffers to the disk.
 */

int
TDUpdate()
{
    register struct IOExtTD *req = DiskIOReq;

    req->iotd_Req.io_Command = ETD_UPDATE;

    return MyDoIO(req);
}
#endif

int
MyDoIO(ioreq)
register struct IOStdReq *ioreq;
{
    ioreq->io_Flags |= IOF_QUICK;	/* Preserve IOMDF_40TRACKS */
    BeginIO(ioreq);
    return WaitIO(ioreq);
}
