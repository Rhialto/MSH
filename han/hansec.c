/*-
 * $Id: hansec.c,v 1.33 91/01/24 00:09:38 Rhialto Exp $
 * $Log:	hansec.c,v $
 * Revision 1.33  91/01/24  00:09:38  Rhialto
 * Constrain behaviour of FindFreeSector.
 *
 * Revision 1.32  90/11/23  23:53:51  Rhialto
 * Prepare for syslog
 *
 * Revision 1.31  90/11/10  02:44:35  Rhialto
 * Patch 3a. Changes location of disk volume date.
 *
 * Revision 1.30  90/06/04  23:17:02  Rhialto
 * Release 1 Patch 3
 *
 * HANSEC.C
 *
 * The code for the messydos file system handler.
 *
 * Sector-level stuff: read, write, cache, unit conversion.
 * Other interactions (via MyDoIO) with messydisk.device.
 *
 * This code is (C) Copyright 1989-1991 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include "dos.h"
#include "han.h"

#ifdef HDEBUG
#   define	debug(x)  syslog x
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
struct Cache	CacheList;	/* Sector cache */
int		CurrentCache;	/* How many cached buffers do we have */
int		MaxCache;	/* Maximum amount of cached buffers */
long		CacheBlockSize; /* Size of disk block + overhead */
ulong		BufMemType;
int		DelayState;
short		CheckBootBlock; /* Do we need to check the bootblock? */

#define LRU_TO_SEC(lru) ((struct CacheSec *)((char *)lru - \
			OFFSETOF(CacheSec, sec_LRUNode)))
#define NN_TO_SEC(nn)   ((struct CacheSec *) nn)

word
Get8086Word(Word8086)
register byte  *Word8086;
{
    return Word8086[0] | Word8086[1] << 8;
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

    return (entry = GetFatEntry(cluster)) >= 0xFFF7 ? FAT_EOF : entry;
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

/*
 * FindFreeSector is like FindFreeCluster, but communicates in terms of
 * sector numbers instead of cluster numbers. This is only useful for
 * directories, since they count in sector numbers because the root
 * directory cannot be expressed in clusters. If a new sector is allocated,
 * the rest of its cluster is allocated as well, of course. The returned
 * sector is always the first sector of a cluster.
 */

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
 * For convenience we remember the preceeding cached sector, for the
 * common case that we want to insert a new sector.
 */

struct MinNode *PredNumberNode;

struct CacheSec *
FindSecByNumber(number)
register int	number;
{
    register struct CacheSec *sec;
    register struct MinNode  *nextsec;

    debug(("FindSecByNumber %ld ", (long)number));

    /*
     * IF the most recently used sector has a number not higher than
     * the one we want, start looking there instead of at the
     * lowest sector number.
     * Following the LRU chain further is not so effective since it
     * has a decreasing tendency.
     */
    {
	register struct CacheSec *lrusec;

	if (CurrentCache > 0 &&
	    (lrusec = LRU_TO_SEC(CacheList.LRUList.mlh_Head),
	    lrusec->sec_Number <= number)) {
	    sec = lrusec;
	} else
	    sec = NN_TO_SEC(CacheList.NumberList.mlh_Head);
    }

    while (nextsec = sec->sec_NumberNode.mln_Succ) {
	if (sec->sec_Number == number) {
	    debug((" (%lx) %lx\n", (long)sec->sec_Refcount, sec));
	    Remove(&sec->sec_LRUNode);
	    AddHead(&CacheList.LRUList, &sec->sec_LRUNode);
	    return sec;
	}
	debug(("cache %ld %lx; ", (long)sec->sec_Number, sec));
	if (sec->sec_Number > number) {
	    /* We need to insert before this one */
	    debug(("insert b4 %ld ", (long)sec->sec_Number));
	    break;
	}
#ifdef notdef	/* This improvement is at best marginal I think */
	{
	    register struct CacheSec *lrusec;

	    if ((lrusec = NextNode(&sec->sec_LRUNode)) &&
		(lrusec = LRU_TO_SEC(lrusec),
		lrusec->sec_Number > sec->sec_Number) &&
		lrusec->sec_Number <= number) {
		sec = lrusec;
		debug(("++LRU++ "));
	    } else
		sec = NN_TO_SEC(nextsec);
	}
#else
	sec = NN_TO_SEC(nextsec);
#endif
    }

    /*
     * If we ran off the end of the list, or if it was empty,
     * *sec is now the dummy end marker. In all 3 cases we need its
     * predecessor.
     */

    PredNumberNode = sec->sec_NumberNode.mln_Pred;
    debug(("sec = %lx, pred = %lx; ", sec, PredNumberNode));
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
NewCacheSector(pred)
struct MinNode *pred;
{
    register struct CacheSec *sec;
    register struct MinNode  *nextsec;

    debug(("NewCacheSector\n"));

    if (CurrentCache < MaxCache) {
	if (sec = AllocMem(CacheBlockSize, BufMemType)) {
	    goto add;
	}
    }
    for (sec = LRU_TO_SEC(CacheList.LRUList.mlh_TailPred);
	 nextsec = sec->sec_LRUNode.mln_Pred;
	 sec = LRU_TO_SEC(nextsec)) {
	if ((CurrentCache >= MaxCache) && (sec->sec_Refcount == SEC_DIRTY)) {
	    FreeCacheSector(sec);       /* Also writes it to disk */
	    continue;
	}
	if (sec->sec_Refcount == 0) {    /* Implies not SEC_DIRTY */
	    Remove(&sec->sec_LRUNode);
	    Remove(&sec->sec_NumberNode);
	    goto move;
	}
    }

    sec = AllocMem(CacheBlockSize, BufMemType);

    if (sec) {
add:
	CurrentCache++;
move:
	AddHead(&CacheList.LRUList, &sec->sec_LRUNode);
	Insert(&CacheList.NumberList, &sec->sec_NumberNode, pred);
    } else
	error = ERROR_NO_FREE_STORE;

    debug(("NewCacheSector: %lx\n", sec));
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
    debug(("FreeCacheSector %ld\n", (long)sec->sec_Number));
    Remove(&sec->sec_LRUNode);
    Remove(&sec->sec_NumberNode);
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

    NewList(&CacheList.LRUList);
    NewList(&CacheList.NumberList);
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

    debug(("FreeCacheList, %ld\n", (long)CurrentCache));
    while (sec = GetHead(&CacheList.NumberList)) {
	FreeCacheSector(NN_TO_SEC(sec));
    }
}

/*
 * Write all dirty cache buffers to disk. They are written from highest to
 * lowest, and then the FAT is written out.
 */

void
MSUpdate(immediate)
int		immediate;
{
    register struct CacheSec *sec;
    register void  *nextsec;

    debug(("MSUpdate\n"));

#ifndef READONLY
    if (DelayState & DELAY_DIRTY) {
	/*
	 * Do a backward scan to write them out.
	 */
	for (sec = NN_TO_SEC(CacheList.NumberList.mlh_TailPred);
	     nextsec = (void *) sec->sec_NumberNode.mln_Pred;
	     sec = NN_TO_SEC(nextsec)) {
	    if (sec->sec_Refcount & SEC_DIRTY) {
		PutSec(sec->sec_Number, sec->sec_Data);
		sec->sec_Refcount &= ~SEC_DIRTY;
	    }
	}
	DelayState &= ~DELAY_DIRTY;
    }
    if (FatDirty) {
	WriteFat();
    }
#endif

    if (immediate)
	DelayState = DELAY_RUNNING1;

    if (DelayState & DELAY_RUNNING2) {
	StartTimer();
	DelayState &= ~DELAY_RUNNING2;
    } else {			/* DELAY_RUNNING1 */
#ifndef READONLY
	while (TDUpdate() != 0 && RetryRwError(DiskIOReq))
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

#ifdef HDEBUG
    if (sector == 0) {
	debug(("************ GetSec(0) ***************\n"));
    }
#endif

    if (sec = FindSecByNumber(sector)) {
	sec->sec_Refcount++;

	return sec->sec_Data;
    }
    if (sec = NewCacheSector(PredNumberNode)) {
	register struct IOExtTD *req;

	sec->sec_Number = sector;
	sec->sec_Refcount = 1;

	debug(("GetSec %ld\n", (long)sector));

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

#ifdef HDEBUG
    if (sector == 0) {
	debug(("************ EmptySec(0) ***************\n"));
    }
#endif
    if (sec = FindSecByNumber(sector)) {
	sec->sec_Refcount++;

	return sec->sec_Data;
    }
    if (sec = NewCacheSector(PredNumberNode)) {
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

    debug(("PutSec %ld\n", (long)sector));

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
#ifdef HDEBUG
	if (sec->sec_Number == 0) {
	    debug(("************ FreeSec(0) ***************\n"));
	}
#endif
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
AwaitDFx()
{
    debug(("AwaitDFx\n"));
    if (DosType) {
	static char	dfx[] = "DFx:";
	void	       *dfxProc,
		       *DeviceProc();
	char		xinfodata[sizeof(struct InfoData) + 3];
	struct InfoData *infoData;
	int		triesleft;

	dfx[2] = '0' + UnitNr;
	infoData = (struct InfoData *)(((long)&xinfodata[3]) & ~3L);

	for (triesleft = 10; triesleft; triesleft--) {
	    debug(("AwaitDFx %ld\n", (long)triesleft));
	    if ((dfxProc = DeviceProc(dfx)) == NULL)
		break;

	    dos_packet(dfxProc, ACTION_DISK_INFO, CTOB(infoData));
	    debug(("AwaitDFx %lx\n", infoData->id_DiskType));
	    if (infoData->id_DiskType == ID_NO_DISK_PRESENT) {
		/* DFx has not noticed yet. Wait a bit. */
		WaitIO(TimeIOReq);
		TimeIOReq->tr_node.io_Command = TR_ADDREQUEST;
		TimeIOReq->tr_time.tv_secs = 0;
		TimeIOReq->tr_time.tv_micro = 750000L;	/* .75 s */
		SendIO(TimeIOReq);
		continue;
	    }
	    if (infoData->id_DiskType == ID_DOS_DISK) {
		/* DFx: understands it, so it is not for us. */
		return 1;
	    }
	    /*
	     * All (well, most) other values mean that DFx: does not
	     * understand it, so we can give it a try.
	     */
	    break;
	}
    }
    return 0;
}

int
ReadBootBlock()
{
    int protstatus;

    debug(("ReadBootBlock\n"));
    FreeFat();                  /* before disk parameters change */
    TDClear();

    if (TDProtStatus() >= 0) {
	register byte *bootblock;
	short	    oldCancel;

	oldCancel = Cancel;

	if (AwaitDFx())
	    goto bad_disk;
	if ((protstatus = TDProtStatus()) < 0)
	    goto no_disk;

	TDChangeNum();
	debug(("Changenumber = %ld\n", DiskIOReq->iotd_Count));

	Cancel = 1;
	if (bootblock = GetSec(0)) {
	    word bps;

	    if (CheckBootBlock &&
				/* Atari: empty or 68000 JMP */
		/*bootblock[0] != 0x00 && bootblock[0] != 0x4E &&*/
				/* 8086 ml for a jump */
		bootblock[0] != 0xE9 && bootblock[0] != 0xEB) {
		goto bad_disk;
	    }
	    bps = Get8086Word(bootblock + 0x0b);
	    Disk.spc = bootblock[0x0d];
	    Disk.res = Get8086Word(bootblock + 0x0e);
	    Disk.nfats = bootblock[0x10];
	    Disk.ndirs = Get8086Word(bootblock + 0x11);
	    Disk.nsects = Get8086Word(bootblock + 0x13);
	    Disk.media = bootblock[0x15];
	    Disk.spf = Get8086Word(bootblock + 0x16);
	    Disk.spt = Get8086Word(bootblock + 0x18);
	    Disk.nsides = Get8086Word(bootblock + 0x1a);
	    Disk.nhid = Get8086Word(bootblock + 0x1c);
	    FreeSec(bootblock);

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
/*	    Disk.fat16bits = Disk.nsects > 20740;   /* DOS3.2 magic value */
	    Disk.fat16bits = Disk.maxclst >= 0xFF7; /* DOS3.0 magic value */

	    debug(("%lx\tbytes per sector\n", (long)Disk.bps));
	    debug(("%lx\tsectors per cluster\n", (long)Disk.spc));
	    debug(("%lx\treserved blocks\n", (long)Disk.res));
	    debug(("%lx\tfats\n", (long)Disk.nfats));
	    debug(("%lx\tdirectory entries\n", (long)Disk.ndirs));
	    debug(("%lx\tsectors\n", (long)Disk.nsects));
	    debug(("%lx\tmedia byte\n", (long)Disk.media));
	    debug(("%lx\tsectors per FAT\n", (long)Disk.spf));
	    debug(("%lx\tsectors per track\n", (long)Disk.spt));
	    debug(("%lx\tsides\n", (long)Disk.nsides));
	    debug(("%lx\thidden sectors\n", (long)Disk.nhid));

	    debug(("%lx\tdirectory sectors\n", (long)Disk.ndirsects));
	    debug(("%lx\troot dir block\n", (long)Disk.rootdir));
	    debug(("%lx\tblock for (imaginary) cluster 0\n", (long)Disk.start));
	    debug(("%lx\tfirst data block\n", (long)Disk.datablock));
	    debug(("%lx\tclusters total\n", (long)Disk.maxclst));
	    debug(("%lx\tbytes per cluster\n", (long)Disk.bpc));
	    debug(("%lx\t16-bits FAT?\n", (long)Disk.fat16bits));

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
	    debug(("Can't read %ld.\n", (long)DiskIOReq->iotd_Req.io_Error));
	bad_disk:
	    FreeCacheList();
	    error = ERROR_NO_DISK;
	    IDDiskType = ID_UNREADABLE_DISK;
	    IDDiskState = ID_WRITE_PROTECTED;
	}
	Cancel = oldCancel;
    }
#ifdef HDEBUG
    else debug(("No disk inserted %ld.\n", (long)DiskIOReq->iotd_Req.io_Error));
#endif
no_disk:
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
		    break;
		}
		dirent++;
	    }
	    strncpy(name, Disk.vollabel.de_Msd.msd_Name, 8 + 3);
	    name[8 + 3] = '\0';
	    ZapSpaces(name, name + 8 + 3);
	    ToDateStamp(date, Disk.vollabel.de_Msd.msd_CreationDate,
			Disk.vollabel.de_Msd.msd_CreationTime);
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
