/*-
 * $Id: hansec.c,v 1.54 1993/06/24 05:12:49 Rhialto Exp $
 * $Log: hansec.c,v $
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * try heuristics for the cache. DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:27:32  Rhialto
 * No real change.
 *
 * Revision 1.51  92/04/17  15:37:19  Rhialto
 * Freeze for MAXON.
 *
 * Revision 1.46  91/10/06  18:25:31  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.43  91/09/28  01:38:43  Rhialto
 * Changed to newer syslog stuff.
 *
 * Revision 1.42  91/06/13  23:48:16  Rhialto
 * DICE conversion; fix cache bug
 *
 * Revision 1.40  91/03/03  18:36:08  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.35  91/03/03  17:42:19  Rhialto
 * Cache list is now two lists: LRU and sorted by sector.
 *
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
 * This code is (C) Copyright 1989-1992 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include <string.h>
#include "han.h"
#include "dos.h"

#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

Prototype struct MsgPort *DiskReplyPort;
Prototype struct IOExtTD *DiskIOReq;
Prototype struct IOStdReq *DiskChangeReq;
Prototype struct DiskParam DefaultDisk;
Prototype struct DiskParam Disk;
Prototype struct Partition Partition;
Prototype byte *Fat;
Prototype short FatDirty;
Prototype short error;
Prototype long	IDDiskState;
Prototype long	IDDiskType;
Prototype struct timerequest *TimeIOReq;
Prototype int	MaxCache;
Prototype ulong BufMemType;
Prototype char	CacheDirty;
Prototype char	DelayCount;
Prototype short CheckBootBlock;
Prototype word	Get8086Word(byte *Word8086);
Prototype word	OtherEndianWord(long oew);     /* long should become word */
Prototype ulong OtherEndianLong(ulong oel);
#ifndef OtherEndianMsd
/*Prototype void  OtherEndianMsd (struct MsDirEntry *msd);*/
#endif
Prototype word	ClusterToSector(word cluster);
Prototype word	ClusterOffsetToSector(word cluster, word offset);
Prototype word	DirClusterToSector(word cluster);
Prototype word	SectorToCluster(word sector);
Prototype word	NextCluster(word cluster);
Prototype word	NextClusteredSector(word sector);
Prototype word	FindFreeSector(word prev);
Prototype struct CacheSec *FindSecByNumber(int number);
Prototype struct CacheSec *FindSecByBuffer(byte *buffer);
Prototype struct CacheSec *NewCacheSector(struct MinNode *pred);
Prototype void	FreeCacheSector(struct CacheSec *sec);
Prototype void	InitCacheList(void);
Prototype void	FreeCacheList(void);
Prototype void	MSUpdate(int immediate);
Prototype void	StartTimer(int);
Prototype byte *ReadSec(int sector);
Prototype byte *EmptySec(int sector);
Prototype void	WriteSec(int sector, byte *data);
Prototype void	MayWriteTrack(struct CacheSec *cache);
Prototype void	FreeSec(byte *buffer);
Prototype void	MarkSecDirty(byte *buffer);
Prototype void	WriteFat(void);
Prototype int	AwaitDFx(void);
Prototype int	ReadBootBlock(void);
Prototype int	IdentifyDisk(char *name, struct DateStamp *date);
Prototype void	TDRemChangeInt(void);
Prototype int	TDAddChangeInt(struct Interrupt *interrupt);
Prototype int	TDChangeNum(void);
Prototype int	TDProtStatus(void);
Prototype int	TDMotorOff(void);
Prototype int	TDClear(void);
Prototype int	TDUpdate(void);
Prototype int	MyDoIO(struct IOStdReq *ioreq);

struct MsgPort *DiskReplyPort;
struct IOExtTD *DiskIOReq;
struct IOStdReq *DiskChangeReq;
int		HeadOnTrack;

struct DiskParam DefaultDisk = {
    MS_BPS,
    MS_SPC,
    MS_RES,
    MS_NFATS,
    MS_NDIRS,
    MS_NSECTS,
    0,	    /* MEDIA */
    MS_SPF,
    MS_SPT,
    MS_NSIDES,
    0,	    /* NHID */
};

struct DiskParam Disk;
struct Partition Partition;
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
char		CacheDirty;	/* Cache must be written to disk */
char		DelayCount;
short		CheckBootBlock; /* Do we need to check the bootblock? */

#define LRU_TO_SEC(lru) ((struct CacheSec *)((char *)lru - \
			OFFSETOF(CacheSec, sec_LRUNode)))
#define NN_TO_SEC(nn)   ((struct CacheSec *) nn)

long dos_packet1(struct MsgPort *port, long type, long arg1);

#if 0
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
     return  (oew << 8) |
	    ((oew >> 8) & 0xff);
}

ulong
OtherEndianLong(oel)
ulong		oel;
{
     return ((oel &       0xff) << 24) |
	    ((oel &     0xff00) <<  8) |
	    ((oel &   0xff0000) >>  8) |
	    ((oel & 0xff000000) >> 24);

}
#endif

#ifndef OtherEndianMsd
void
OtherEndianMsd(msd)
register struct MsDirEntry *msd;
{
    msd->msd_Date = OtherEndianWord(msd->msd_Date);
    msd->msd_Time = OtherEndianWord(msd->msd_Time);
    msd->msd_Cluster = OtherEndianWord(msd->msd_Cluster);
    msd->msd_Filesize = OtherEndianLong(msd->msd_Filesize);
}
#endif

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
 * Claims that the file ends if something strange happens.
 */

word
NextCluster(cluster)
word cluster;
{
    register word entry;

    entry = GetFatEntry(cluster);
    if (entry >= 0xFFF7 || entry == 0 || entry > Disk.maxclst)
	return FAT_EOF;
    else
	return entry;
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
	    Remove((struct Node *)&sec->sec_LRUNode);
	    AddHead((struct List *)&CacheList.LRUList,
		    (struct Node *)&sec->sec_LRUNode);
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
    return (struct CacheSec *) (buffer - OFFSETOF(CacheSec, sec_Data[0]));
}

/*
 * Get a fresh cache buffer. If we are allowed more cache, we just
 * allocate memory. Otherwise, we try to find a currently unused buffer.
 * We start looking at the end of the list, which is the bottom of the LRU
 * stack. If that fails, allocate more memory anyway. Not that is likely
 * anyway, since we currently lock only one sector at a time.
 *
 * The desired predecessor in the numerically sorted list is given so
 * we don't have to look it up again.
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
	    /*
	     * If the predecessor is being (re)moved, it is no longer a
	     * stable point to attach the new or recycled sector to so we
	     * need to step back on the list.
	     */
	    if (&sec->sec_NumberNode == pred)
		pred = sec->sec_NumberNode.mln_Pred;
	    debug(("NewCacheSector: dump dirty sec %d\n", sec->sec_Number));
	    FreeCacheSector(sec);       /* Also writes it to disk */
	    continue;
	}
	if (sec->sec_Refcount == 0) {   /* Implies not SEC_DIRTY */
	    if (&sec->sec_NumberNode == pred) /* Same comment */
		pred = sec->sec_NumberNode.mln_Pred;
	    debug(("NewCacheSector: re-use clean sec %d\n", sec->sec_Number));
	    Remove((struct Node *)&sec->sec_LRUNode);
	    Remove((struct Node *)&sec->sec_NumberNode);
	    goto move;
	}
    }

    sec = AllocMem(CacheBlockSize, BufMemType);

    if (sec) {
add:
	CurrentCache++;
move:
	AddHead((struct List *) &CacheList.LRUList,
		(struct Node *) &sec->sec_LRUNode);
	Insert((struct List *) &CacheList.NumberList,
	       (struct Node *) &sec->sec_NumberNode,
	       (struct Node *) pred);
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

    if (sec->sec_Refcount & ~SEC_DIRTY) {
	debug(("\n\t*** PANIC!!! Refcount not 0 !!! (%x) ***\n\n", sec->sec_Refcount));
	sec->sec_Refcount &= SEC_DIRTY;
    }

#ifndef READONLY
    if (sec->sec_Refcount & SEC_DIRTY) {
	/*WriteSec(sec->sec_Number, sec->sec_Data);*/
	MayWriteTrack(sec);
    }
#endif
    Remove((struct Node *)&sec->sec_LRUNode);
    Remove((struct Node *)&sec->sec_NumberNode);
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

    NewList((struct List *)&CacheList.LRUList);
    NewList((struct List *)&CacheList.NumberList);
    CurrentCache = 0;
    CacheDirty = 0;
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
 * Eventually write all dirty cache buffers to disk. When we decide to
 * do this, we start writing at the nearest end of the range we need
 * to go through.
 */

void
MSUpdate(immediate)
int		immediate;
{
    int 	    writingfat;

    debug(("MSUpdate, imm=%d, count=%d\n", immediate, DelayCount));

    if (immediate)
	DelayCount = 1;

    if (DelayCount == 0)
	return;

#ifndef READONLY
    writingfat = DelayCount <= 2 && FatDirty;

    if (DelayCount <= 3 && CacheDirty) {
	register struct CacheSec *sec;
	register struct MinNode *nextsec;
	int		lowtrack, hightrack;
	int		offset;

	if (CurrentCache > 0) {
	    if (writingfat) {
		lowtrack = 0;
	    } else {
		sec = NN_TO_SEC(CacheList.NumberList.mlh_Head);
		lowtrack = sec->sec_Number / Disk.spt;
	    }
	    sec = NN_TO_SEC(CacheList.NumberList.mlh_TailPred);
	    hightrack = sec->sec_Number / Disk.spt;

	    if ((HeadOnTrack - lowtrack) <= (hightrack - HeadOnTrack)) {
		/* closer to low track */
		debug(("MSUpdate: Closer to lower track %d <- %d %d\n",
		       lowtrack, HeadOnTrack, hightrack));
		sec = NN_TO_SEC(CacheList.NumberList.mlh_Head);
		offset = OFFSETOF(CacheSec, sec_NumberNode.mln_Succ);
		if (writingfat) {
		    WriteFat();
		}
	    } else {
		/* closer to high track */
		debug(("MSUpdate: Closer to higher track %d %d -> %d\n",
		       lowtrack, HeadOnTrack, hightrack));
		sec = NN_TO_SEC(CacheList.NumberList.mlh_TailPred);
		offset = OFFSETOF(CacheSec, sec_NumberNode.mln_Pred);
	    }

	    for ( ; nextsec = *(struct MinNode **)((char *)sec + offset);
		 sec = NN_TO_SEC(nextsec)) {
		if (sec->sec_Refcount & SEC_DIRTY) {
		    WriteSec(sec->sec_Number, sec->sec_Data);
		    sec->sec_Refcount &= ~SEC_DIRTY;
		}
	    }
	}
	CacheDirty = 0;
    }
    if (writingfat)
	WriteFat();
#endif

    if (DelayCount <= 1) {
#ifndef READONLY
	debug(("MSUpdate, do TDUpdate\n"));
	while (TDUpdate() != 0 && RetryRwError(DiskIOReq))
	    ;
#endif
	TDMotorOff();
    }
    if (--DelayCount)
	StartTimer(0);
}

/*
 * Start the timer which triggers cache writing and stopping the disk
 * motor.
 */

void
StartTimer(times)
int		times;
{
    if (DelayCount < times)
	DelayCount = times;

    if (CheckIO((struct IORequest *)TimeIOReq)) {
	WaitIO((struct IORequest *)TimeIOReq);
	TimeIOReq->tr_node.io_Command = TR_ADDREQUEST;
	TimeIOReq->tr_time.tv_secs = 2;
	TimeIOReq->tr_time.tv_micro = 0;
	SendIO((struct IORequest *)TimeIOReq);
    }
}

/*
 * Get a pointer to a logical sector { 0, ..., MS_SECTCNT - 1}. We
 * allocate a buffer and copy the data in, and lock the buffer until
 * FreeSec() is called.
 */

byte	       *
ReadSec(sector)
int		sector;
{
    struct CacheSec *sec;

#ifdef HDEBUG
    if (sector == 0) {
	debug(("************ ReadSec(0) ***************\n"));
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

	debug(("ReadSec %ld\n", (long)sector));

	req = DiskIOReq;
	do {
	    req->iotd_Req.io_Command = ETD_READ;
	    req->iotd_Req.io_Data = (APTR)sec->sec_Data;
	    req->iotd_Req.io_Offset = Partition.offset + (long) sector * Disk.bps;
	    req->iotd_Req.io_Length = Disk.bps;
	    MyDoIO(&req->iotd_Req);
	} while (req->iotd_Req.io_Error != 0 && RetryRwError(req));

	StartTimer(2);
	HeadOnTrack = sector / Disk.spt;

	if (req->iotd_Req.io_Error == 0) {
	    return sec->sec_Data;
	}
	error = ERROR_NOT_A_DOS_DISK;
	sec->sec_Refcount = 0;
	FreeCacheSector(sec);
    }
    return NULL;
}

#ifndef READONLY

byte	       *
EmptySec(sector)
int		sector;
{
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
WriteSec(sector, data)
int		sector;
byte	       *data;
{
    register struct IOExtTD *req;

    debug(("WriteSec %ld\n", (long)sector));

    req = DiskIOReq;
    do {
	req->iotd_Req.io_Command = ETD_WRITE;
	req->iotd_Req.io_Data = (APTR) data;
	req->iotd_Req.io_Offset = Partition.offset + (long) sector * Disk.bps;
	req->iotd_Req.io_Length = Disk.bps;
	MyDoIO(&req->iotd_Req);
    } while (req->iotd_Req.io_Error != 0 && RetryRwError(req));

    StartTimer(2);
    HeadOnTrack = sector / Disk.spt;
}

/*
 * Write all sectors that are on the same track as this one.
 * This speeds up I/O by taking advantage of track buffering and
 * reduction of seeking.
 * As a general rule we don't write sectors which are dirty but
 * still in use. (Not that this is expected to occur often, though).
 */

void
MayWriteTrack(cache)
struct CacheSec *cache;
{
    struct CacheSec *c;
    struct MinNode *cn;
    int 	    lowsec;
    int 	    highsec;

    if (CacheDirty == 0)
	return;

    lowsec = (cache->sec_Number / Disk.spt) * Disk.spt;
    highsec = lowsec + Disk.spt;

    debug(("MayWriteTrack sec %d (sec %d-%d)\n", cache->sec_Number, lowsec, highsec-1));

    for (c = cache; cn = c->sec_NumberNode.mln_Pred; c = NN_TO_SEC(cn)) {
	if (c->sec_Number < lowsec)
	    break;
	cache = c;
    }

    for (c = cache; cn = c->sec_NumberNode.mln_Succ; c = NN_TO_SEC(cn)) {
	if (c->sec_Number >= highsec)
	    break;
	if (c->sec_Refcount == SEC_DIRTY) {     /* don't write active sectors */
	    WriteSec(c->sec_Number, c->sec_Data);
	    c->sec_Refcount &= ~SEC_DIRTY;
	}
    }
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

    if (buffer) {
	register struct CacheSec *sec;

	sec = FindSecByBuffer(buffer);
#ifdef HDEBUG
	if (sec->sec_Number == 0) {
	    debug(("************ FreeSec(0) ***************\n"));
	}
#endif
	--sec->sec_Refcount;
#if 1
	/* Write out the sector, if doing it now is cheap */
	if (/*(sec->sec_Refcount == SEC_DIRTY) &&*/
	    (sec->sec_Number / Disk.spt) == HeadOnTrack) {
	    /*WriteSec(sec->sec_Number, sec->sec_Data);*/
	    /*sec->sec_Refcount &= ~SEC_DIRTY;*/
	    MayWriteTrack(sec);
	}
#endif
#ifdef notdef
	if (sec->sec_Refcount == 0) { /* Implies not SEC_DIRTY */
	    if (CurrentCache > MaxCache) {
		FreeCacheSector(sec);
	    }
	}
#else
	/*
	 * If we need to dump cache then dump some long-unused sectors.
	 */
	while (CurrentCache > MaxCache &&
	    (sec = LRU_TO_SEC(GetTail(&CacheList.LRUList))) &&
	    (sec->sec_Refcount & ~SEC_DIRTY) == 0) {
	    debug(("FreeSec: dump %s sec %d\n",
		    (sec->sec_Refcount & SEC_DIRTY)? "dirty" : "clean",
		    sec->sec_Number));
	    FreeCacheSector(sec);
	}
#endif
    }
}

#ifndef READONLY

void
MarkSecDirty(buffer)
byte	       *buffer;
{
    register struct CacheSec *sec;

    if (buffer) {
	sec = FindSecByBuffer(buffer);
	sec->sec_Refcount |= SEC_DIRTY;
	CacheDirty = 1;
	StartTimer(4);
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
	    WriteSec(disksec++, Fat + sec * Disk.bps);
	}
    }
    FatDirty = FALSE;
}

#endif

long
dos_packet1(port, type, arg1)
struct MsgPort *port;
long type, arg1;
{
    struct StandardPacket *sp;
    struct MsgPort *rp;
    long res1;

    if ((rp = CreatePort(NULL, 0L)) == NULL)
	return DOS_FALSE;
    if ((sp = AllocMem((long)sizeof(*sp), MEMF_PUBLIC|MEMF_CLEAR)) == NULL) {
	DeletePort(rp);
	return DOS_FALSE;
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
AwaitDFx()
{
    debug(("AwaitDFx\n"));
    if (Interleave & NICE_TO_DFx) {
	static char	dfx[] = "DFx:";
	void	       *dfxProc;
	char		xinfodata[sizeof(struct InfoData) + 3];
	struct InfoData *infoData;
	int		triesleft;

	dfx[2] = '0' + UnitNr;
	infoData = (struct InfoData *)(((long)&xinfodata[3]) & ~3L);

	if ((dfxProc = DeviceProc(dfx)) == NULL)
	    return 0;

	for (triesleft = 10; triesleft; triesleft--) {
	    debug(("AwaitDFx %ld\n", (long)triesleft));

	    dos_packet1(dfxProc, ACTION_DISK_INFO, (long)CTOB(infoData));
	    debug(("AwaitDFx %lx\n", infoData->id_DiskType));
	    if (infoData->id_DiskType == ID_NO_DISK_PRESENT) {
		/* DFx has not noticed yet. Wait a bit. */
		WaitIO((struct IORequest *)TimeIOReq);
		TimeIOReq->tr_node.io_Command = TR_ADDREQUEST;
		TimeIOReq->tr_time.tv_secs = 0;
		TimeIOReq->tr_time.tv_micro = 750000L;	/* .75 s */
		SendIO((struct IORequest *)TimeIOReq);
		continue;
	    }
	    if ((infoData->id_DiskType & 0xFFFFFF00) == ID_DOS_DISK) {
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
	if (bootblock = ReadSec(0)) {
	    word	oldbps;

	    if ((CheckBootBlock & CHECK_BOOTJMP) &&
				/* Atari: empty or 68000 JMP */
		bootblock[0] != 0x00 && bootblock[0] != 0x4E &&
				/* 8086 ml for a jump */
		bootblock[0] != 0xE9 && bootblock[0] != 0xEB) {

		FreeSec(bootblock);
		IDDiskType = ID_NOT_REALLY_DOS;
		goto not_dos_disk;
	    }
	    oldbps = Disk.bps;
	    if (CheckBootBlock & CHECK_USE_DEFAULT) {
		Disk = DefaultDisk;
	    } else {
		Disk.bps = Get8086Word(bootblock + 0x0b);
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
	    }
	    FreeSec(bootblock);

	recalculate:
	    /*
	     *	Maybe the sector size just changed. Who knows?
	     */
	    if (Disk.bps != oldbps) {
		FreeCacheList();
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
/*	    Disk.fat16bits = Disk.nsects > 20740;  / * DOS3.2 magic value */
	    Disk.fat16bits = Disk.maxclst >= 0xFF7; /* DOS3.0 magic value */

	    /*
	     * We set this for the benefit of ouside programs; this value
	     * will not be used directly by us, since it is reset to one
	     * of the defaults on every disk change or format.
	     */
	    if (Environ)
		Environ->de_BlocksPerTrack = Disk.spt;

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

	    /*
	     * Sanity check.
	     */
	    if (CheckBootBlock & CHECK_SANITY) {
		if (Disk.spc < 1 ||
		    Disk.nfats < 1 ||
		    Disk.ndirs < 8 ||
		    Disk.nsects < 640 ||
		    Disk.spf < 1 ||
		    Disk.spt < 1 ||
		    Disk.nsides < 1 ||
		    Disk.ndirsects < 1) {
		    if (CheckBootBlock & CHECK_SAN_DEFAULT) {
			debug(("Bad bootblock; using default values.\n"));
			oldbps = Disk.bps;
			Disk = DefaultDisk;
			goto recalculate;
		    } else {
			IDDiskType = ID_NOT_REALLY_DOS;
			goto not_dos_disk;
		    }
		}
	    }

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
	    IDDiskType = ID_UNREADABLE_DISK;
	not_dos_disk:
	    FreeCacheList();
	    error = ERROR_NO_DISK;
	    IDDiskState = ID_VALIDATING;
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

	if (dirblock = ReadSec(Disk.rootdir)) {
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
	    strncpy(name, Disk.vollabel.de_Msd.msd_Name, L_8 + L_3);
	    name[L_8 + L_3] = '\0';
	    ZapSpaces(name, name + L_8 + L_3);
	    ToDateStamp(date, msd_CreationDate(Disk.vollabel.de_Msd),
			      msd_CreationTime(Disk.vollabel.de_Msd));
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
	MyDoIO(&req->iotd_Req);
	WaitIO(&DiskChangeReq->iotd_Req);
#else
	Forbid();
	Remove(&DiskChangeReq->io_Message.mn_Node);
	Permit();
#endif
	DeleteExtIO((struct IORequest *)DiskChangeReq);
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
	SendIO((struct IORequest *)DiskChangeReq);

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
    MyDoIO(&req->iotd_Req);
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
    MyDoIO(&req->iotd_Req);

    if (req->iotd_Req.io_Error)
	return -1;

    return req->iotd_Req.io_Actual != 0;
}

/*
 * Switch the drive motor off. Return previous state.
 */

int
TDMotorOff()
{
    register struct IOExtTD *req = DiskIOReq;

    req->iotd_Req.io_Command = TD_MOTOR;
    req->iotd_Req.io_Length = 0;
    MyDoIO(&req->iotd_Req);

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

    return MyDoIO(&req->iotd_Req);
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

    return MyDoIO(&req->iotd_Req);
}
#endif

int
MyDoIO(ioreq)
register struct IOStdReq *ioreq;
{
    ioreq->io_Flags |= IOF_QUICK;	/* Preserve IOMDF_40TRACKS */
    BeginIO((struct IORequest *)ioreq);
    return WaitIO((struct IORequest *)ioreq);
}
