/*-
 *  $Id: han.h,v 1.53 92/10/25 02:44:29 Rhialto Rel $
 *  $Log:	han.h,v $
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
 *  This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 *  not be used or copied without a licence.
-*/

#include "dev.h"

#ifndef CLIB_EXEC_PROTOS_H
#include <clib/exec_protos.h>
#endif
#ifndef CLIB_ALIB_PROTOS_H
#include <clib/alib_protos.h>
#endif

extern struct ExecBase *SysBase;

/*----- Configuration section -----*/

#define CONVERSIONS
#undef	NONCOMM
#undef	READONLY

/*----- End configuration section -----*/

/* #define MODE_READWRITE  1004L */
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

#define MS_FIRSTCLUST	2	/* Very strange convention... */

#define FAT_EOF     0xFFFF	/* end of file FAT entry */
#define FAT_UNUSED  0		/* unused block */
#define SEC_EOF     ((word)-1)  /* end of FAT chain */
#define ROOT_SEC    ((word)-1)  /* where the root directory 'is' */

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
    word	    freeclusts; /* amount of free space */
    long	    lowcyl;	/* offset to lowcyl */
    struct DirEntry vollabel;	/* copy of volume label */
    word	    fat16bits;	/* Is the FAT 16 bits/entry? */
};

#define CHECK_BOOTJMP	0x01	/* accept disk only with JMP or 00 */
#define CHECK_SANITY	0x02	/* check Bios Parameter Block */
#define CHECK_SAN_DEFAULT 0x04	/* use default values when bpb not ok */
#define CHECK_USE_DEFAULT 0x08	/* always use default values */

#define NICE_TO_DFx	(1L<<16)/* flag bit in de_Interleave */
#define PROMISE_NOT_TO_DIE  (1L<<17)/* flag bit in de_Interleave */

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
    word	    msfl_DirSector;	/* Location of directory entry */
    word	    msfl_DirOffset;
    word	    msfl_Flags;
};

#define MSFL_DIRTY  0x01		/* Only used in MSWrite */

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

struct PrivateInfo {
    short	    Revision;
    short	    Size;
    char	   *RCSId;
    short	   *CheckBootBlock;
    short	   *DefaultConversion;
    struct IOExtTD **DiskIOReq;
#ifdef CONVERSIONS
    short	    NumConversions;
    struct {
	unsigned char **to, **from;
    }		    Table[2];
#endif
};

#define PRIVATE_REVISION    2

#ifndef Prototype
#define Prototype   extern
#endif
#ifndef Local
#define Local	    static
#endif

#include "hanproto.h"
