/*-
 * $Id: han2.c,v 1.55 1993/12/30 23:28:00 Rhialto Rel $
 *
 * The code for the messydos file system handler.
 *
 * New functions to make 2.0+ stuff work.
 *
 * This code is (C) Copyright 1991-1994 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
 *
 * $Log: han2.c,v $
 * Revision 1.55  1993/12/30  23:28:00	Rhialto
 * Freeze for MAXON5.
 * Add MSFormat().
 * Lots of small changes for LONGNAMES option.
 * Fix MSSameLock(), because TADM was wrong again.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:43:47  Rhialto
 * Make it work.
 *
 * Revision 1.52  92/09/06  00:20:34  Rhialto
 *
 * Revision 1.46  91/10/06  18:26:04  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.43  91/09/28  01:30:07  Rhialto
 * Preliminary - not functional yet
-*/

#include "han.h"
#include "dos.h"
#include <string.h>

#ifdef ACTION_SAME_LOCK      /* Do we have 2.0 includes? */

#if HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

Prototype long	MSSameLock(struct MSFileLock *lock1, struct MSFileLock *lock2);
Prototype struct MSFileHandle *MSOpenFromLock(struct MSFileLock *lock);
Prototype struct MSFileLock *MSDupLockFromFH(struct MSFileHandle *msfh);
Prototype struct MSFileLock *MSParentOfFH(struct MSFileHandle *msfh);
Prototype long	MSExamineFH(struct MSFileHandle *msfh, struct FileInfoBlock *fib);
Prototype long	MSSetFileSize(struct MSFileHandle *msfh, long pos, long mode);
Prototype long	MSChangeModeLock(struct MSFileLock *object, long newmode);
Prototype long	MSChangeModeFH(struct MSFileHandle *object, long newmode);
Prototype long	MSFormat(char *vol, long type);
Prototype long	MSSerializeDisk(void);

/*
 * We are taking great care to share lock structures between locks on
 * the same file. It is here, among other places, that we exploit that.
 */
long
MSSameLock(lock1, lock2)
struct MSFileLock *lock1, *lock2;
{
    return (lock1 == lock2);
}

struct MSFileHandle *
MSOpenFromLock(lock)
struct MSFileLock *lock;
{
    return MakeMSFileHandle(lock, MODE_OLDFILE);
    /* The lock is now owned by the MSFileHandle */
}

/*
 * For DupLockFromFH and ParentOfFH we force the user to insert the disk.
 * This is the only easy way of knowing the VolumeNode from the filehandle,
 * and we need it in the FileLock. Yech!
 */

struct MSFileLock *
MSDupLockFromFH(msfh)
struct MSFileHandle *msfh;
{
    if (CheckLock(msfh->msfh_FileLock) == 0) {
	debug(("MSDupLockFromFH\n"));
	return MSDupLock(msfh->msfh_FileLock);
    } else {
	debug(("MSDupLockFromFH, fails\n"));
	return NULL;
    }
}

struct MSFileLock *
MSParentOfFH(msfh)
struct MSFileHandle *msfh;
{
    if (CheckLock(msfh->msfh_FileLock) == 0) {
	debug(("MSParentOfFH\n"));
	return MSParentDir(msfh->msfh_FileLock);
    } else {
	debug(("MSParentOfFH, fails\n"));
	return NULL;
    }
}

long
MSExamineFH(msfh, fib)
struct MSFileHandle *msfh;
struct FileInfoBlock *fib;
{
    return MSExamine(msfh->msfh_FileLock, fib);
}

/*
 * MSSetFileSize.
 *
 * For the case that we shorten a file and invalidate existing seek pointer,
 * there is a test in MSRead(), MSWrite() and MSSeek() which detects this
 * condition. It is not worth (yet?) to keep a list of file handles
 * so we can do the same test here.
 */

long
MSSetFileSize(msfh, pos, mode)
struct MSFileHandle *msfh;
long		pos;
long		mode;
{
    long	    oldclusters,
		    newclusters;
    struct MSFileLock *msfl;
    long	    success = DOSFALSE;

    msfl = msfh->msfh_FileLock;

    if (msfl->msfl_Msd.msd_Attributes & ATTR_READONLY) {
	debug(("MSSetFileSize on writeprotected file\n"));
	error = ERROR_WRITE_PROTECTED;
	return DOSFALSE;
    }

    AdjustSeekPos(msfh);
    pos = FilePos(msfh, pos, mode);

    if (pos < 0) {
	error = ERROR_SEEK_ERROR;
	return DOSFALSE;
    }

    oldclusters = (msfl->msfl_Msd.msd_Filesize + Disk.bpc - 1) / Disk.bpc;
    newclusters = (pos + Disk.bpc - 1) / Disk.bpc;

    debug(("MSSetFileSize to %ld (%ld clusters from %ld)\n",
	    pos, newclusters, oldclusters));
    if (newclusters == oldclusters) {
	/* do nothing */
    } else if (newclusters > oldclusters) {
	/* extend the file */
	word	    cluster;

	newclusters -= oldclusters;
	if (newclusters > Disk.freeclusts) {
	    /* Make file as long as will fit on the disk */
	    newclusters = Disk.freeclusts;
	    pos = (oldclusters + newclusters) * Disk.bpc;
	    debug(("File growth reduced to %d clusters\n", newclusters));
	    error = ERROR_DISK_FULL;
	}

	cluster = msfl->msfl_Msd.msd_Cluster;
	debug(("Begin with cluster %d\n", cluster));

	if (oldclusters == 0 && newclusters > 0) {
	    cluster = FindFreeCluster(0);
	    debug(("  initial cluster %d\n", cluster));
	    msfl->msfl_Msd.msd_Cluster = cluster;
	    newclusters--;
	}
	while (newclusters > 0) {
	    cluster = ExtendClusterChain(cluster);
	    debug(("  add cluster %d\n", cluster));
	    newclusters--;
	}
    } else if (newclusters == 0) {
	/* make the file empty */

	FreeClusterChain(msfl->msfl_Msd.msd_Cluster);
	msfl->msfl_Msd.msd_Cluster = 0;
    } else {
	/* shorten the file. This may invalidate seek pointers...  */
	word	    cluster;

	cluster = msfl->msfl_Msd.msd_Cluster;
	oldclusters = 1;
	while (oldclusters < newclusters) {
	    cluster = NextCluster(cluster);
	    oldclusters++;
	}
	if (cluster != FAT_EOF) {
	    FreeClusterChain(NextCluster(cluster));
	    SetFatEntry(cluster, FAT_EOF);
	}
    }

    msfl->msfl_Msd.msd_Filesize = pos;
#if CREATIONDATE_ONLY
    DirtyFileLock(msfl);
#else
    UpdateFileLock(msfl);
#endif
    AdjustSeekPos(msfh);

    return DOSTRUE;
}

long
MSChangeModeLock(object, newmode)
struct MSFileLock *object;
long		newmode;
{
    /* We DON'T lock directories exclusively! */
    if (object->msfl_Msd.msd_Attributes & ATTR_DIR)
	newmode = SHARED_LOCK;

    if (newmode == EXCLUSIVE_LOCK) {
	if (object->msfl_Refcount <= 1) {
	    object->msfl_Refcount = -1;
	    return DOSTRUE;
	}
	error = ERROR_OBJECT_IN_USE;
    } else { /* SHARED_LOCK */
	if (object->msfl_Refcount == -1) {
	    object->msfl_Refcount = 1;
	}
	return DOSTRUE;
    }

    return DOSFALSE;
}

long
MSChangeModeFH(object, newmode)
struct MSFileHandle *object;
long		newmode;
{
    if (newmode == MODE_NEWFILE)
	newmode = EXCLUSIVE_LOCK;
    else    /* MODE_OLDFILE, MODE_READWRITE */
	newmode = SHARED_LOCK;

    return MSChangeModeLock(object->msfh_FileLock, newmode);
}

/*
 * ExAll can conveniently be done with Examine/ExNext... at least for now.
 */

#ifdef notdef

#include <dos/exall.h>
long
MSExAll(lock, ead, size, data, eac)
struct MSFileLock  *lock;
struct ExAllData   *ead;
long		    size;
long		    data;
struct ExAllControl *eac;
{
    struct FileInfoBlock fib;
    char	   *next;
    struct Hook    *h;
    char	   *end;

    eac->eac_Entries = 0;
    next = ead;
    h = eac->eac_MatchFunc;
    end = (char *)ead + size;
    if (Examine(lock, &fib)) {
	while (((char *)(ead + 1) + L_8+1+L_3 + 1) <= end &&
	       ExNext(lock, &fib)) {
	    if (h) {
		if (!(h->h_SubEntry(h->h_Data)))    /* ??? */
		    continue;
	    }

	    if (data >= ED_NAME) {
		if (data >= ED_TYPE) {
		    ead->ed_Type = fib.fib_DirEntryType;
		    if (data >= ED_SIZE) {
			ead->ed_Size = fib.fib_Size;
			if (data >= ED_PROTECTION) {
			    ead->ed_Protection = fib.fib_Protection;
			    if (data >= ED_DATE) {
				ead->ed_Days = fib.fib_Date.Days;
				ead->ed_Mins = fib.fib_Date.Mins;
				ead->ed_Ticks = fib.fib_Date.Ticks;
				if (data >= ED_COMMENT) {
				    ead->ed_Comment = NULL;
				    next = &ead->ed_Comment + 1;
				} else
				    next = &ead->ed_Comment;
			    } else
				next = &ead->ed_Protection;
			} else
			    next = &ead->ed_Protection;
		    } else
			next = &ead->ed_Size;
		} else
		    next = &ead->ed_Type;
		ead->ed_Name = next;
		strcpy(next, fib.fib_FileName + 1);
		next += strlen(next) + 1;
	    } else
		next = &ead->ed_Name;

	    ead = (struct ExAllData *)(((long)next + 1) & ~1);
	    eac->eac_Entries++;
	} /* while ExNext */
	return 0;   /* ??? */
    }
    return 0;	/* ??? */
}

#endif

const ulong	BootBlock[] = {
    0xEB349049, 0x424D2020, 0x332E3200, 0x02020100,	/* ...IBM  3.2..... */
#if 0
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
#endif
};

void
PutWord(byte *address, word value)
{
    address[0] = value;
    address[1] = value >> 8;
}

/*
 * Format a disk. Low-level format has already been done, so we just
 * write out the bootblock, FAT and root directory.
 *
 * We assume all default values, because there is no way to communicate
 * anything special. Keep using MSH-Format for special needs.
 *
 * The only adjustments we make are for 40/80 tracks and DD/HD disks.
 *
 * fs is the device name, but is ignored and NULL for now.
 * vol is the desired volume name.
 * type is the (file system private) file system type, and is ignored.
 */

long
MSFormat(char *vol, long type)
{
    byte	   *sec;
    int 	    n;
    int 	    i, j;
    ULONG	    success = DOSFALSE;

    CheckDriveType();
    Disk = DefaultDisk;

    debug(("MSFormat: getting boot block\n"));
    n = 0;
    sec = EmptySec(n);
    memset(sec, 0, DefaultDisk.bps);

    CopyMem(BootBlock, sec, sizeof(BootBlock));

    debug(("%d sectors on disk, %d per track\n",
	   DefaultDisk.nsects, DefaultDisk.spt));

	  *(sec + 0x00) = 0x00; 	/* Not bootable. */

    PutWord(sec + 0x0b,   DefaultDisk.bps);
	  *(sec + 0x0d) = DefaultDisk.spc;
    PutWord(sec + 0x0e,   DefaultDisk.res);
	  *(sec + 0x10) = DefaultDisk.nfats;
    PutWord(sec + 0x11,   DefaultDisk.ndirs);
    PutWord(sec + 0x13,   DefaultDisk.nsects);
	  *(sec + 0x15) = DefaultDisk.media;
    PutWord(sec + 0x16,   DefaultDisk.spf);
    PutWord(sec + 0x18,   DefaultDisk.spt);
    PutWord(sec + 0x1a,   DefaultDisk.nsides);
    PutWord(sec + 0x1c,   DefaultDisk.nhid);

    MarkSecDirty(sec);
    FreeSec(sec);

    /* Go to first FAT: skip bootblock. */

    n += DefaultDisk.res;

    for (i = 0; i < DefaultDisk.nfats; i++) {
	debug(("MSFormat: getting first sector of FAT #%d\n", i));
	sec = EmptySec(n++);
	memset(sec, 0, DefaultDisk.bps);

	sec[0] = 0xF9;
	sec[1] = 0xFF;
	sec[2] = 0xFF;

	MarkSecDirty(sec);
	FreeSec(sec);

	for (j = 1; j < DefaultDisk.spf; j++) {
	    debug(("MSFormat: getting sector %d of FAT #%d\n", j, i));
	    sec = EmptySec(n++);
	    memset(sec, 0, DefaultDisk.bps);
	    MarkSecDirty(sec);
	    FreeSec(sec);
	}
    }

    /* Clear entire root directory. */
    for (i = (DefaultDisk.ndirs*MS_DIRENTSIZE + DefaultDisk.bps-1) / DefaultDisk.bps;
	 i > 0; i--) {
	debug(("MSFormat: getting root dir sector %d\n", n));
	sec = EmptySec(n++);
	memset(sec, 0, DefaultDisk.bps);
	MarkSecDirty(sec);
	FreeSec(sec);
    }

    /* Label the disk. This is ugly. */

    debug(("MSFormat: Labeling the disk\n"));
    {
	int		old_inhibited = Inhibited;
	long		old_interleave = Interleave;

	Inhibited = 0;
	Interleave &= ~NICE_TO_DFx;
	MSUpdate(1);	/* Normally implied by DiskChange(), but because we
			 * may be Inhibit()ed, this may not happen. */
	DiskChange();
	if (RootLock)
	    success = MSRelabel(vol);
	else
	    debug(("*** No root lock yet!\n"));
	Inhibited = old_inhibited;
	Interleave = old_interleave;
	DiskChange();
    }

    return success;
}

long
MSSerializeDisk(void)
{
    int 	    old_inhibited = Inhibited;
    long	    old_interleave = Interleave;
    ULONG	    success = DOSFALSE;

    Inhibited = 0;
    Interleave &= ~NICE_TO_DFx;
    DiskChange();
    if (RootLock) {
	char name[L_8 + L_3 + 1];
	strncpy(name, Disk.vollabel.de_Msd.msd_Name, L_8 + L_3);

	/* Relabel with the same name... ugly. */
	success = MSRelabel(name);
    } else
	debug(("*** No root lock yet!\n"));
    Inhibited = old_inhibited;
    Interleave = old_interleave;
    DiskChange();

    return success;
}

#endif /* ACTION_COMPARE_LOCK */
