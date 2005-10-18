/*-
 * $Id: han.h,v 1.58 2005/10/19 16:53:52 Rhialto Exp $
 *
 * $Log: han.h,v $
 * Revision 1.58  2005/10/19  16:53:52  Rhialto
 * Finally a new version!
 *
 * Revision 1.56  1996/12/22  00:22:33  Rhialto
 * Cosmetics.
 *
 * Revision 1.55  1993/12/30  23:28:00	Rhialto
 * Freeze for MAXON5.
 * Add compile time options LONGNAMES and CREATIONDATE_ONLY.
 * Move lowcyl from Disk to Partition.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:44:29  Rhialto
 * Add PrivateInfo. #define magic cookie.
 *
 * Revision 1.52  92/09/06  00:06:08  Rhialto
 * Add cast to *_EOFs.
 *
 * Revision 1.51  92/04/17  15:39:01  Rhialto
 * Freeze for MAXON3.
 *
 * Revision 1.46  91/10/06  18:26:52  Rhialto
 *
 * Freeze for MAXON
 *
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
 *  This code is (C) Copyright 1989-1997 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#define SysBase_DECLARED

#include "dev.h"

#ifndef CLIB_EXEC_PROTOS_H
#include <clib/exec_protos.h>
#endif
#ifndef CLIB_ALIB_PROTOS_H
#include <clib/alib_protos.h>
#endif

extern struct ExecBase *SysBase;

/*----- Configuration section -----*/

#define CONVERSIONS		1
#define NONCOMM 		0
#define READONLY		0
#define INPUTDEV		1
#define CREATIONDATE_ONLY	1
#define TASKWAIT		1
#define VFATSUPPORT		1

/*----- End configuration section -----*/

#if VFATSUPPORT
/*
 * For now, long file names are not compatible with the syntax
 * for character conversion.
 */
#undef CONVERSIONS
#define CONVERSIONS		0
#endif

#define MODE_CREATEFILE (1L<<31)
#define FILE_DIR     2
#define FILE_FILE   -3

#define MS_SPC	    2		/* Sectors per cluster */
#define MS_RES	    1		/* Reserved sectors (boot block) */
#define MS_NFATS    2		/* Number of FATs */
#define MS_NDIRS    112 	/* Number of directory entries */
#define MS_NSECTS   1440	/* total number of sectors */
#define MS_SPF	    3		/* Sectors per FAT */
#define MS_ROOTDIR  (MS_RES + MS_SPF * MS_NFATS)
#define MS_DIRENTSIZE  sizeof(struct MsDirEntry) /* size of a directory entry */
#define MS_DIRENT_SHIFT	5	/* 1 << MS_DIRENT_SHIFT == MS_DIRENTSIZE */

#define MS_FIRSTCLUST	2	/* Very strange convention... */

#define FAT_EOF     ((cluster_t)0xFFFF)	/* end of file FAT entry */
#define FAT_UNUSED  0		/* unused block */
#define SEC_EOF     ((sector_t)-1)	/* end of FAT chain */
#define ROOT_SEC    ((sector_t)-1)	/* where the root directory 'is' */

#define DIR_DELETED	    0xE5
#define DIR_DELETED_MASK    0x80
#define DIR_E5_REPLACEMENT  0x05
#define DIR_END		    0x00

#if VFATSUPPORT
#define INITIAL_MAX_CACHE	25
#else
#define INITIAL_MAX_CACHE	5
#endif

typedef word	cluster_t;
typedef ulong	sector_t;

#if LONGNAMES
struct MsDirEntry {
    word	    msd_ModTime;
    word	    msd_ModDate;
    word	    msd_Cluster;
    ulong	    msd_Filesize;
    byte	    msd_Attributes;
    byte	    msd_Name[21];	    /* Note: odd aligned */
};
#define 	    L_8 	21
#define 	    L_3 	0
#define 	    OtherEndianMsd(e)
#define 	    msd_CreationTime(e) (*(word *)(&(e).msd_Name[17]))
#define 	    msd_CreationDate(e) (*(word *)(&(e).msd_Name[19]))
#else
/*
 * This structure has its byte order wrong, when it is on the disk.
 */

struct MsDirEntry {
    byte	    msd_Name[8];
    byte	    msd_Ext[3];
    byte	    msd_Attributes;
    byte	    msd_Case;
    byte	    msd_CreationTimems;
    word	    msd_CreationTime;	    /* vollabel only */
    word	    msd_CreationDate;	    /* vollabel only */
    word	    msd_ClusterHigh; 	    /* not msd_AccessTime */
    word	    msd_AccessDate;
    word	    msd_ModTime;
    word	    msd_ModDate;
    word	    msd_Cluster;
    ulong	    msd_Filesize;
};
#define 	    L_8 	8
#define 	    L_3 	3
void  OtherEndianMsd(struct MsDirEntry *msd);
#define 	    msd_CreationTime(e) ((e).msd_CreationTime)
#define 	    msd_CreationDate(e) ((e).msd_CreationDate)
#endif

#define ATTR_READONLY	    0x01
#define ATTR_HIDDEN	    0x02
#define ATTR_SYSTEM	    0x04
#define ATTR_VOLUMELABEL    0x08
#define ATTR_DIRECTORY	    0x10
#define ATTR_ARCHIVE	    0x20

#define ATTR_WIN95	    0x0F	    /* RO | HIDDEN | SYSTEM | LABEL */
#define ATTR_DIR	    (ATTR_DIRECTORY | ATTR_VOLUMELABEL)

#define DATE_MIN	    0x21

#define BASECASE	    0x10	    /* base name is lowercase */
#define EXTCASE		    0x08	    /* extension is lowercase */

#if VFATSUPPORT

struct MsVfatSubEntry {
    byte	    se_Count;
#define SE_LAST	    0x40
#define SE_COUNT    0x3F
    byte	    se_Part1[10];
    byte	    se_Attributes;
    byte	    se_Pad1;
    byte	    se_Checksum;
    byte	    se_Part2[12];
    word	    se_Pad2;
    byte	    se_Part3[4];
};
#define SE_CHARS    13			    /* number of chars per SubEntry */
#define SE_MAX_ENTRIES	8		    /* We do names up to 104 chars */
#define SE_MAX_CHARS	(SE_CHARS*SE_MAX_ENTRIES) /* due to the size of
					     * fib_Filename[108] */

#endif

struct DirEntry {
    struct MsDirEntry de_Msd;
    sector_t	    de_Sector;
    int		    de_Offset;
#if VFATSUPPORT
    sector_t	    de_VfatnameSector;
    int		    de_VfatnameOffset;
#endif
};

struct DiskParam {
    int		    bps;	/* bytes per sector */
    int		    spc;	/* byte: sectors per cluster */
    sector_t	    res;	/* reserved sectors (boot block) */
    int		    nfats;	/* byte: number of fats */
    int		    ndirs;	/* number of directory entries */
    sector_t	    nsects;	/* total number of sectors on disk */
    int		    media;	/* byte: media byte */
    int		    spf;	/* sectors per fat */
    int		    spt;	/* sectors per track */
    int		    nsides;	/* # sides */
    sector_t	    nhid;	/* Number of hidden sectors */
    /* derived parameters */
    sector_t	    start;	/* sector of cluster 0 */
    cluster_t	    maxclst;	/* highest cluster number */
    sector_t	    rootdir;	/* first sector of root dir */
    sector_t	    ndirsects;	/* # of root directory sectors */
    sector_t	    datablock;	/* first block available for files &c */
    int		    bpc;	/* bytes per cluster */
    cluster_t	    freeclusts; /* amount of free space */
    struct DirEntry vollabel;	/* copy of volume label */
    int		    fatbits;	/* Is the FAT 12 or 16 bits/entry? */
    int		    dos5;	/* is this a DOS 5+ disk? */
    int		    examinesecshift;
};

#define CHECK_BOOTJMP	0x01	/* accept disk only with JMP or 00 */
#define CHECK_SANITY	0x02	/* check Bios Parameter Block */
#define CHECK_SAN_DEFAULT 0x04	/* use default values when bpb not ok */
#define CHECK_USE_DEFAULT 0x08	/* always use default values */

struct Partition {
    long	    offset;	/* offset to sector 0 in bytes */
    int 	    spt_dd;	/* normal #sectors/track */
    int 	    spt_hd;	/* #s/t for HD floppies */
};

#define OPT_NICE_TO_DFx	(1L<<16)/* flag bit in de_Interleave */
#define OPT_PROMISE_NOT_TO_DIE  (1L<<17)/* flag bit in de_Interleave */
#define OPT_NO_WIN95	(1L<<18)
#define OPT_SHORTNAME	(1L<<19)

#define MSH_MAGIC	'Msh\0' /* Magic word in DosPackets */

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
    sector_t	    msfl_DirSector;	/* Location of directory entry */
    int		    msfl_DirOffset;
#if VFATSUPPORT
    sector_t	    msfl_VfatnameSector;
    int		    msfl_VfatnameOffset;
#endif
    word	    msfl_Flags;
};

#define MSFL_DIRTY  0x01		/* Only used in MSWrite */

/*
 * For Examine()/ExNext() we need to encode the current location
 * in 32 bits, and also a flag to record if we need to enter
 * into a directory (when Examine()ing a directory for the first time).
 *
 * We assume: 32 bytes per directory entry,
 * hence 5 low-order offset bits that are always 0.
 */

#define PACK_EXAMINE_LOCATION(sec, off) \
	((sec << Disk.examinesecshift) | (off >> 4))
#define UNPACK_EXAMINE_SECTOR(loc) \
	(loc >> Disk.examinesecshift)
#define UNPACK_EXAMINE_OFFSET(loc) \
	((loc & ((1L << Disk.examinesecshift)-1-1)) << 4)
	/* extra -1 to mask out the flag bit */

/*
 * A pointer to an MSFileHandle is put into the fh_Arg1 field of a DOS
 * FileHandle. We get that value with many DOS packets that manipulate the
 * contents of a file.
 */

struct MSFileHandle {
    struct MSFileLock *msfh_FileLock;
    long	    msfh_SeekPos;
    cluster_t	    msfh_Cluster;
#if CONVERSIONS
    int 	    msfh_Conversion;
#endif
};

struct LockList {
    struct MinList  ll_List;
    void	   *ll_Cookie;	/* we don't want to know what this is! */
};

struct CacheSec {
    struct MinNode  sec_NumberNode;
    struct MinNode  sec_LRUNode;
    sector_t	    sec_Number;
    word	    sec_Refcount;
    byte	    sec_Data[2];/* Really Disk.bps */
};

#define SEC_DIRTY   0x8000	/* Bit in sec_Refcount */

struct Cache {
    struct MinList  LRUList;
    struct MinList  NumberList;
};

#define OFFSETOF(tag, member)	((long)(&((struct tag *)0)->member))

struct PrivateInfo {
    short	    Revision;
    short	    Size;
    char	   *RCSId;
    short	   *CheckBootBlock;
    short	   *DefaultConversion;
    struct IOExtTD **DiskIOReq;
    short	    NumConversions;
    struct {
	unsigned char **to, **from;
    }		    Table[2];
};

/*
 * Constants for unix2dos
 */
#define U2D_CANNOT_CONVERT	0
#define U2D_CONVERTED_SAME	1
#define U2D_CONVERTED_OK	2
#define U2D_CONVERTED_GENNR	3
#define U2D_CONVERTED_TRUNC	4

#define PRIVATE_REVISION    2

#ifndef Prototype
#define Prototype   extern
#endif
#ifndef Local
#define Local	    static
#endif

#include "hanproto.h"
