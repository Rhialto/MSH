/*-
 *  $Id: han.h,v 1.43 91/09/28 01:50:02 Rhialto Exp $
 *  $Log:	han.h,v $
 * Revision 1.43  91/09/28  01:50:02  Rhialto
 * Byteswap routines no longer __stkargs.
 *
 * Revision 1.42  91/06/14  00:09:19  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:55:38  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.35  91/03/03  17:43:39  Rhialto
 * Cache list is now two lists: LRU and sorted by sector.
 *
 * Revision 1.31  90/11/10  02:50:47  Rhialto
 * Patch 3a. Introduce disk volume date.
 *
 * Revision 1.30  90/06/04  23:18:28  Rhialto
 * Release 1 Patch 3
 *
 *  The header file for the MESSYDOS: file system handler
 *
 *  This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 *  not be used or copied without a licence.
-*/

#include "dev.h"

/*----- Configuration section -----*/

#define CONVERSIONS
#undef	NONCOMM
#undef	READONLY

/*----- End configuration section -----*/

#define MODE_READWRITE	1004L
#define MODE_CREATEFILE (1L<<31)
#define FILE_DIR     2
#define FILE_FILE   -3

/* #define MS_BPS      512	/* Bytes per sector */
#define MS_SPC	    2		/* Sectors per cluster */
#define MS_RES	    1		/* Reserved sectors (boot block) */
#define MS_NFATS    2		/* Number of FATs */
#define MS_NDIRS    112 	/* Number of directory entries */
#define MS_NSECTS   1440	/* total number of sectors */
#define MS_SPF	    3		/* Sectors per FAT */
/* #define MS_SPT      9	/* Sectors per track */
/* #define MS_SPT_MAX  9	/* Max sectors per track */
/* #define MS_NSIDES   2	/* Tracks per cylinder */
#define MS_ROOTDIR  (MS_RES + MS_SPF * MS_NFATS)
#define MS_DIRENTSIZE  sizeof(struct MsDirEntry) /* size of a directory entry */

#define MS_FIRSTCLUST	2	/* Very strange convention... */

#define FAT_EOF     0xFFFF	/* end of file FAT entry */
#define FAT_UNUSED  0		/* unused block */
#define SEC_EOF     -1		/* end of FAT chain */
#define ROOT_SEC    -1		/* where the root directory 'is' */

#define DIR_DELETED	    0xE5
#define DIR_DELETED_MASK    0x80

/*
 * This structure has its byte order wrong, when it is on the disk.
 */

struct MsDirEntry {
    byte	    msd_Name[8];
    byte	    msd_Ext[3];
    byte	    msd_Attributes;
    word	    msd_CreationTime;	    /* vollabel only */
    word	    msd_CreationDate;	    /* vollabel only */
    byte	    msd_Pad1[6];
    word	    msd_Time;
    word	    msd_Date;
    word	    msd_Cluster;
    ulong	    msd_Filesize;
};

#define ATTR_READONLY	    0x01
#define ATTR_HIDDEN	    0x02
#define ATTR_SYSTEM	    0x04
#define ATTR_VOLUMELABEL    0x08
#define ATTR_DIRECTORY	    0x10
#define ATTR_ARCHIVED	    0x20

#define ATTR_DIR	    (ATTR_DIRECTORY | ATTR_VOLUMELABEL)

#define DATE_MIN	    0x21

struct DirEntry {
    struct MsDirEntry de_Msd;
    word	    de_Sector;
    word	    de_Offset;
};

struct DiskParam {
    word	    bps;	/* bytes per sector */
    word	    spc;	/* byte: sectors per cluster */
    word	    res;	/* reserved sectors (boot block) */
    word	    nfats;	/* byte: number of fats */
    word	    ndirs;	/* number of directory entries */
    word	    nsects;	/* total number of sectors on disk */
    word	    media;	/* byte: media byte */
    word	    spf;	/* sectors per fat */
    word	    spt;	/* sectors per track */
    word	    nsides;	/* # sides */
    word	    nhid;	/* Number of hidden sectors */
    /* derived parameters */
    word	    start;	/* sector of cluster 0 */
    word	    maxclst;	/* highest cluster number */
    word	    rootdir;	/* first sector of root dir */
    word	    ndirsects;	/* # of root directory sectors */
    word	    datablock;	/* first block available for files &c */
    word	    bpc;	/* bytes per cluster */
    word	    nsectsfree; /* amount of free space */
    long	    lowcyl;	/* offset to lowcyl */
    struct DirEntry vollabel;	/* copy of volume label */
    word	    fat16bits;	/* Is the FAT 16 bits/entry? */
};

#define CHECK_BOOTJMP	0x01	/* accept disk only with JMP or 00 */
#define CHECK_SANITY	0x02	/* check Bios Parameter Block */
#define CHECK_SAN_DEFAULT 0x04	/* use default values when bpb not ok */
#define CHECK_USE_DEFAULT 0x08	/* always use default values */


/*
 * A pointer to an MSFileLock is put into the fl_Key field of a DOS
 * FileLock structure. We share the MSFileLock with all FileLocks on the
 * same file. This way, you can compare FileLocks based on their fl_Key
 * and fl_Task fields, as seems to be done sometimes. Also, a pointer to
 * an MSFileLock is put in MSFileHandles.
 *
 * For ease, we keep a copy of the directory entry in core, WITH THE BYTE
 * ORDER CORRECTED FOR THIS PROCESSOR.
 */

struct MSFileLock {
    struct MinNode  msfl_Node;
    short	    msfl_Refcount;	/* -1: exclusive, >0: # of shared
					 * locks */
    struct MSFileLock *msfl_Parent;	/* Pointer to parent directory */
    struct MsDirEntry msfl_Msd; 	/* Copy of directory entry */
    word	    msfl_DirSector;	/* Location of directory entry */
    word	    msfl_DirOffset;
};

/*
 * A pointer to an MSFileHandle is put into the fh_Arg1 field of a DOS
 * FileHandle. We get that value with many DOS packets that manipulate the
 * contents of a file.
 */

struct MSFileHandle {
    struct MSFileLock *msfh_FileLock;
    long	    msfh_SeekPos;
    word	    msfh_Cluster;
#ifdef CONVERSIONS
    int 	    msfh_Conversion;
#endif
};

/*
 * Return values of CompareNames.
 */

#define CMP_NOT_EQUAL	    0	/* Names do not match at all */
#define CMP_OK_DIR	    1	/* Name matches with a subdir entry */
#define CMP_OK_FILE	    2	/* Name matches with a file entry */
#define CMP_INVALID	    3	/* First part of name matches with a file
				 * entry, or other invalid component name */
#define CMP_FREE_SLOT	    5
#define CMP_END_OF_DIR	    6

struct LockList {
    struct MinList  ll_List;
    void	   *ll_Cookie;	/* we don't want to know what this is! */
};

struct CacheSec {
    struct MinNode  sec_NumberNode;
    struct MinNode  sec_LRUNode;
    word	    sec_Number;
    word	    sec_Refcount;
    byte	    sec_Data[2];/* Really Disk.bps */
};

#define SEC_DIRTY   0x8000	/* Bit in sec_Refcount */

struct Cache {
    struct MinList  LRUList;
    struct MinList  NumberList;
};

#define OFFSETOF(tag, member)   ((long)(&((struct tag *)0)->member))

#define     DELAY_OFF	    0	/* Motor is off */
#define     DELAY_RUNNING1  1	/* Motor may be on */
#define     DELAY_RUNNING2  2	/* Motor may be on */
#define     DELAY_RUNNING   3	/* Running1 | 2 */
#define     DELAY_DIRTY     4	/* We have dirty buffers to flush */


/*
extern long	Wait();
extern struct MsgPort *CreatePort();
extern struct IOExtTD *CreateExtIO();
extern void    *AllocMem(), FreeMem();
extern byte    *index(), *rindex();
extern void    *CheckIO();
extern long	AutoRequest();
*/

/*
 * PACK.C
 */
extern char    *DevName;
extern long	UnitNr;
extern long	DosType;
extern ulong	DevFlags;
extern struct DosPacket *DosPacket;
extern struct DeviceList *VolNode;
extern short	DiskChanged;
int MayFreeVolNode(struct DeviceList *volnode);

/*
 * HANMAIN.C
 */
byte ToUpper(byte ch);
long lmin(long a, long b);
byte *ZapSpaces(byte *begin, byte *end);
byte *ToMSName(byte *dest, byte  *source);
long MSDiskInfo(struct InfoData *infodata);
void MSDiskInserted(struct LockList **locks, void *cookie);
int MSDiskRemoved(struct LockList **locks);
void HanCloseDown(void);
int HanOpenUp(void);
long MSRelabel(byte *newname);

/*
 * HANSEC.C
 */
extern struct MsgPort *DiskReplyPort;
extern struct IOExtTD *DiskIOReq;
extern struct IOStdReq *DiskChangeReq;
extern struct DiskParam Disk;
extern byte    *Fat;
extern short	FatDirty;	/* Fat must be written to disk */
extern short	error;		/* To put the error value; for Result2 */
extern long	IDDiskState;	/* InfoData.id_DiskState */
extern long	IDDiskType;	/* InfoData.id_DiskType */
extern struct timerequest *TimeIOReq;	/* For motor-off delay */
extern struct Cache CacheList;/* Sector cache */
extern int	CurrentCache;	/* How many cached buffers do we have */
extern int	MaxCache;	/* Maximum amount of cached buffers */
extern ulong	BufMemType;
extern long	CacheBlockSize; /* Size of disk block + overhead */
extern int	DelayState;
word Get8086Word(byte *Word8086);
word OtherEndianWord(long oew);     /* long should become word */
ulong OtherEndianLong(ulong oel);
void OtherEndianMsd(struct MsDirEntry *msd);
word ClusterToSector(word cluster);
word ClusterOffsetToSector(word cluster, word offset);
word DirClusterToSector(word cluster);
word SectorToCluster(word sector);
word NextCluster(word cluster);
word NextClusteredSector(word sector);
word FindFreeSector(word prev);
struct CacheSec *FindSecByNumber(int number);
struct CacheSec *FindSecByBuffer(byte *buffer);
struct CacheSec *NewCacheSector(struct MinNode *pred);
void FreeCacheSector(struct CacheSec *sec);
void InitCacheList(void);
void FreeCacheList(void);
void MSUpdate(int immediate);
void StartTimer(void);
byte *GetSec(int sector);
byte *EmptySec(int sector);
void PutSec(int sector, byte *data);
void FreeSec(byte *buffer);
void MarkSecDirty(byte *buffer);
void WriteFat(void);
int AwaitDFx(void);
int ReadBootBlock(void);
int IdentifyDisk(char *name, struct DateStamp *date);
void TDRemChangeInt(void);
int TDAddChangeInt(struct Interrupt *interrupt);
int TDChangeNum(void);
int TDProtStatus(void);
int TDMotorOff(void);
int TDClear(void);
int TDUpdate(void);
int MyDoIO(struct IOStdReq *ioreq);

/*
 * HANLOCK.C
 */
extern struct LockList *LockList;	/* List of all locked files we
					 * have. Note this is not the same
					 * as all locks we have */
extern struct MSFileLock *RootLock;	/* Lock on root directory */
extern struct MSFileLock *EmptyFileLock;	/* 2nd result of MSLock() */

extern struct DirEntry FakeRootDirEntry;
int CompareNames(struct MsDirEntry *dir, byte *name);
void NextDirEntry(word *sector, word *offset);
struct DirEntry *FindNext(struct DirEntry *previous, int createit);
void PrintDirEntry(struct DirEntry *de);
struct MSFileLock *MakeLock(struct MSFileLock *parentdir, struct DirEntry *dir, ulong mode);
struct MSFileLock *MSLock(struct MSFileLock *parentdir, byte *name, ulong mode);
struct MSFileLock *MSDupLock(struct MSFileLock *fl);
struct MSFileLock *MSParentDir(struct MSFileLock *fl);
int MSUnLock(struct MSFileLock *fl);
void ExamineDirEntry(struct MsDirEntry *msd, struct FileInfoBlock *fib);
int MSExamine(struct MSFileLock *fl, struct FileInfoBlock *fib);
int MSExNext(struct MSFileLock *fl, struct FileInfoBlock *fib);
long MSSetProtect(struct MSFileLock *parentdir, char *name, long mask);
int CheckLock(struct MSFileLock *lock);
void WriteFileLock(struct MSFileLock *fl);
void UpdateFileLock(struct MSFileLock *fl);
struct LockList *NewLockList(void *cookie);
void FreeLockList(struct LockList *ll);

/*
 * HANFILE.C
 */
int GetFat(void);
void FreeFat(void);
word GetFatEntry(word cluster);
void SetFatEntry(word cluster, word value);
word FindFreeCluster(word prev);
word ExtendClusterChain(word cluster);
void FreeClusterChain(word cluster);
struct MSFileHandle *MakeMSFileHandle(struct MSFileLock *fl, long mode);
struct MSFileHandle *MSOpen(struct MSFileLock *parentdir, char *name, long mode);
void MSClose(struct MSFileHandle *fh);
long		FilePos(struct MSFileHandle *fh, long position, long mode);
long MSSeek(struct MSFileHandle *fh, long position, long mode);
long MSRead(struct MSFileHandle *fh, byte *userbuffer, long size);
long MSWrite(struct MSFileHandle *fh, byte *userbuffer, long size);
long MSDeleteFile(struct MSFileLock *parentdir, byte *name);
long MSSetDate(struct MSFileLock *parentdir, byte *name, struct DateStamp *datestamp);
struct MSFileLock *MSCreateDir(struct MSFileLock *parentdir, byte *name);
long MSRename(struct MSFileLock *slock, byte *sname, struct MSFileLock *dlock, byte *dname);

/*
 * HANREQ.C
 */
extern short	Cancel; 	/* Cancel all R/W errors */
long RetryRwError(struct IOExtTD *req);
void		DisplayMessage(char *msg);

/*
 * HANCMD.C
 */
void HandleCommand(char *cmd);

/*
 * DATE.C
 */
void ToDateStamp(struct DateStamp *datestamp, word date, word time);
void ToMSDate(word *date, word *time, struct DateStamp *datestamp);
