/*-
 * $Id: hanlock.c,v 1.30a $
 * $Log:	hanlock.c,v $
 * Revision 1.30  90/06/04  23:17:18  Rhialto
 * Release 1 Patch 3
 *
 * HANLOCK.C
 *
 * The code for the messydos file system handler
 *
 * The Lock department. Takes care of operations on locks, and consequently,
 * on directories.
 *
 * This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
-*/

#include "dos.h"
#include "han.h"

#ifdef HDEBUG
#   define	debug(x)  dbprintf x
#else
#   define	debug(x)
#endif

struct LockList *LockList;	/* List of all locked files we have. Note
				 * this is not the same as all locks we
				 * have */
struct MSFileLock *RootLock;	/* Lock on root directory */
struct MSFileLock *EmptyFileLock;	/* 2nd result of MSLock() */

struct DirEntry FakeRootDirEntry = {
    {				/* de_Msd */
	"Unnamed ",             /* msd_Name */
	"   ",                  /* msd_Ext */
	ATTR_VOLUMELABEL,	/* msd_Attributes */
	0,			/* msd_CreationTime */
	DATE_MIN,		/* msd_CreationDate, 1/1/80 */
	{0},			/* msd_Pad1 */
	0,			/* msd_Time */
	DATE_MIN,		/* msd_Date, 1/1/80 */
	0,			/* msd_Cluster */
	0			/* msd_Filesize */
    },
    ROOT_SEC,			/* de_Sector */
    -2				/* de_Offset */
};
byte		DotDot[1 + 8 + 3] = "..          ";

/*
 * This routine compares a name in a directory entry with a given name
 */

int
CompareNames(dir, name)
register struct MsDirEntry *dir;
register byte  *name;
{
    if (dir->msd_Name[0] & DIR_DELETED_MASK)
	return CMP_FREE_SLOT;

    if (dir->msd_Name[0] == 0)
	return CMP_END_OF_DIR;	/* end of directory */

    if (dir->msd_Attributes & ATTR_VOLUMELABEL)
	return CMP_NOT_EQUAL;

    if (strncmp(dir->msd_Name, name, 8 + 3))
	return CMP_NOT_EQUAL;

    if (dir->msd_Attributes & ATTR_DIRECTORY)
	return CMP_OK_DIR;

    return CMP_OK_FILE;
}

void
NextDirEntry(sector, offset)
register word  *sector;
register word  *offset;
{
    if ((*offset += MS_DIRENTSIZE) >= Disk.bps) {
	*offset = 0;
	if (*sector >= Disk.datablock) {
	    /* Must be subdirectory */
	    *sector = NextClusteredSector(*sector);
	    debug(("NextClusteredSector: %d\n", *sector));
	} else {
	    if (++*sector >= Disk.datablock) {
		*sector = SEC_EOF;
	    }
	}
    }
    /* else no more work needed */
}

/*
 * Get the directory entry following the given one. If requested, we make
 * the directory longer.
 */

struct DirEntry *
FindNext(previous, createit)
register struct DirEntry *previous;
int		createit;
{
    byte	   *sector;
    word	    prevsec = previous->de_Sector;

    NextDirEntry(&previous->de_Sector, &previous->de_Offset);

    if (previous->de_Sector == SEC_EOF) {
	error = ERROR_OBJECT_NOT_FOUND;
#ifndef READONLY
	if (createit) {
	    if (prevsec < Disk.datablock - 1) { /* Should not be necessary */
		previous->de_Sector = prevsec + 1;
	    } else if (prevsec >= Disk.datablock) {
		previous->de_Sector = FindFreeSector(prevsec);
	    }
	    if (previous->de_Sector != SEC_EOF) {
		sector = EmptySec(previous->de_Sector);
		setmem(sector, (int) Disk.bps, 0);
		MarkSecDirty(sector);
		FreeSec(sector);
		setmem(&previous->de_Msd, sizeof (previous->de_Msd), 0);

		return previous;
	    }
	}
#endif
    } else if (sector = GetSec(previous->de_Sector)) {
	CopyMem(sector + previous->de_Offset, &previous->de_Msd,
		(long) MS_DIRENTSIZE);
	OtherEndianMsd(&previous->de_Msd);
	FreeSec(sector);

	return previous;
    }
    return NULL;
}

#ifdef HDEBUG

void
PrintDirEntry(de)
struct DirEntry *de;
{
    debug(("%d,%d ", de->de_Sector, de->de_Offset));
    debug(("%.8s.%.3s attr:%x time:%x date:%x start:%x size:%lx\n",
	   de->de_Msd.msd_Name,
	   de->de_Msd.msd_Ext,
	   de->de_Msd.msd_Attributes,
	   de->de_Msd.msd_Time,
	   de->de_Msd.msd_Date,
	   de->de_Msd.msd_Cluster,
	   de->de_Msd.msd_Filesize
	   ));
}

#endif

/*
 * MakeLock makes a struct MSFileLock from a directory entry and the
 * parent directory MSFileLock pointer. It looks if it already has a Lock
 * on it. In that case, it simply increments its reference count, when
 * possible.
 */

struct MSFileLock *
MakeLock(parentdir, dir, mode)
struct MSFileLock *parentdir;
struct DirEntry *dir;
ulong		mode;
{
    register struct MSFileLock *fl;
    struct MSFileLock *nextfl;

    if (mode != EXCLUSIVE_LOCK || (dir->de_Msd.msd_Attributes & ATTR_DIR))
	mode = SHARED_LOCK;

#ifdef HDEBUG
    debug(("MakeLock: "));
    PrintDirEntry(dir);
#endif

    /*
     * Look through our list to see if we already have it. The criteria
     * for this are: 1. the directory entries are the same or 2. they have
     * the same first cluster and are both directories (which can have
     * multiple directory entries). Sigh.
     */

    for (fl = (struct MSFileLock *) LockList->ll_List.mlh_Head;
	 nextfl = (struct MSFileLock *) fl->msfl_Node.mln_Succ;
	 fl = nextfl) {
#ifdef HDEBUG
	debug(("> "));
	PrintDirEntry(&fl->msfl_Msd);
#endif
	if ((fl->msfl_DirSector == dir->de_Sector &&
	     fl->msfl_DirOffset == dir->de_Offset) ||
	    (fl->msfl_Msd.msd_Cluster == dir->de_Msd.msd_Cluster &&
	     (dir->de_Msd.msd_Attributes & ATTR_DIR) &&
	     (fl->msfl_Msd.msd_Attributes & ATTR_DIR))
	    ) {
	    /* Found existing lock on file */
	    if (fl->msfl_Refcount < 0 || mode == EXCLUSIVE_LOCK) {
		error = ERROR_OBJECT_IN_USE;
		return NULL;
	    }
	    fl->msfl_Refcount++;
	    return fl;
	}
    }

    fl = AllocMem((long) sizeof (*fl), MEMF_PUBLIC);
    if (fl == NULL) {
	error = ERROR_NO_FREE_STORE;
	return NULL;
    }
    fl->msfl_Parent = parentdir ? MSDupLock(parentdir) : NULL;

    fl->msfl_Refcount = (mode == EXCLUSIVE_LOCK) ? -1 : 1;
    fl->msfl_DirSector = dir->de_Sector;
    fl->msfl_DirOffset = dir->de_Offset;
    fl->msfl_Msd = dir->de_Msd;

    AddHead(&LockList->ll_List, fl);

    return fl;
}

/*
 * This routine Locks a file. It first searches it in the directory, then
 * lets the rest of the work be done by MakeLock(). If it encounters an
 * empty slot in the directory, it remembers where, in case we need it. If
 * you clear the MODE_CREATEFILE bit in the mode parameter, we fabricate a
 * new MSFileLock from the empty directory entry. It then becomes the
 * caller's responsibility to MSUnLock() it eventually.
 */

struct MSFileLock *
MSLock(parentdir, name, mode)
struct MSFileLock *parentdir;
byte	       *name;
ulong		mode;
{
    byte	   *sector;
    struct MSFileLock *newlock;
    register struct DirEntry *de;
    struct DirEntry sde;
    byte	   *nextpart;
    byte	    component[8 + 3];	/* Note: not null-terminated */
    int 	    createit;
    word	    freesec;
    word	    freeoffset;

    de = &sde;
    newlock = NULL;

    /*
     * See if we have an absolute path name (starting at the root).
     */
    {
	register byte  *colon;

	if (colon = index(name, ':')) {
	    name = colon + 1;
	    parentdir = RootLock;
	    /*
	     * MSH::Command or ::Command?
	     */
	    if (name[0] == ':') {
		HandleCommand(name);
		error = ERROR_OBJECT_NOT_FOUND;

		return NULL;
	    }
	}
    }


    /*
     * Get a copy of the parent dir lock, so we can walk it over the
     * directory tree.
     */
    parentdir = MSDupLock(parentdir);

    /*
     * Start with the directory entry of the parent dir.
     */

    sde.de_Msd = parentdir->msfl_Msd;
    sde.de_Sector = parentdir->msfl_DirSector;
    sde.de_Offset = parentdir->msfl_DirOffset;
#ifdef HDEBUG
    debug(("pdir %08lx: ", parentdir));
    PrintDirEntry(&parentdir->msfl_Msd);
#endif

newdir:
    freesec = SEC_EOF;		/* Means none found yet */

    nextpart = ToMSName(component, name);
    debug(("Component: '%11s'\n", component));
    if (nextpart[0] != '/') {
	nextpart = NULL;
#ifndef READONLY
	/*
	 * See if we are requested to get an empty spot in the directory
	 * if the given name does not exist already. The value of mode is
	 * not important until we actually create the filelock.
	 */
	if (!(mode & MODE_CREATEFILE)) {
	    mode ^= MODE_CREATEFILE;
	    createit = 1;
	} else
	    createit = 0;
#endif
    } else
	nextpart++;

    /*
     * Are we at the end of the name? Then we are finished now. This works
     * because sde contains the directory entry of parentdir.
     */

    if (name[0] == '\0')
	goto exit;

    /*
     * If there is more name, we enter the directory, and here we get the
     * first entry.
     */

    sde.de_Sector = DirClusterToSector(sde.de_Msd.msd_Cluster);
    sde.de_Offset = 0;

    if ((sector = GetSec(sde.de_Sector)) == NULL)
	goto error;

    CopyMem(sector, &sde.de_Msd, (long) sizeof (struct MsDirEntry));
    OtherEndianMsd(&sde.de_Msd);
    FreeSec(sector);

    while (de) {
	switch (CompareNames(&sde.de_Msd, component)) {
	case CMP_FREE_SLOT:
	    if (freesec == SEC_EOF) {
		freesec = sde.de_Sector;
		freeoffset = sde.de_Offset;
	    }
	    /* Fall through */
	case CMP_NOT_EQUAL:
	    de = FindNext(&sde, createit);      /* Try next directory
						 * entry */
	    continue;
	case CMP_OK_DIR:
	    if (name = nextpart) {
		/*
		 * We want to keep locks on all directories between each
		 * bottom directory and file lock, so we can easily find
		 * the parent of a lock. There just seems to be a problem
		 * here when we enter the 'subdirectories' . or .. , but
		 * that in not so: MakeLock will detect that it has
		 * already a lock on those, and NOT make :one/two the
		 * parent of :one/two/.. .
		 */
		newlock = MakeLock(parentdir, de, SHARED_LOCK);
		MSUnLock(parentdir);
		parentdir = newlock;
		goto newdir;
	    }
	    goto exit;
	case CMP_OK_FILE:
	    if (nextpart) {
		error = ERROR_OBJECT_WRONG_TYPE;
		de = NULL;
	    }
	    goto exit;
	case CMP_END_OF_DIR:	/* means we will never find it */
	    error = ERROR_OBJECT_NOT_FOUND;
	    if (freesec == SEC_EOF) {
		freesec = sde.de_Sector;
		freeoffset = sde.de_Offset;
	    }
	    de = NULL;
	    goto exit;
	}
    }

exit:
    if (de) {
	newlock = MakeLock(parentdir, &sde, mode);
    } else {
	newlock = NULL;
#ifndef READONLY
	if (createit &&         /* Do we want to make it? */
	    error == ERROR_OBJECT_NOT_FOUND &&	/* does it not exist yet? */
	    nextpart == NULL) { /* do we have the last part of the name */
	    if (freesec != SEC_EOF) {   /* is there any room? */
		if (IDDiskState == ID_VALIDATED) {
		    error = 0;
		    setmem(&sde.de_Msd, sizeof (sde.de_Msd), 0);
		    sde.de_Sector = freesec;
		    sde.de_Offset = freeoffset;
		    /* ToMSName(sde.de_Msd.msd_Name, name); */
		    strncpy(sde.de_Msd.msd_Name, component, 8 + 3);
		    EmptyFileLock = MakeLock(parentdir, &sde, mode);
		    WriteFileLock(EmptyFileLock);
		} else
		    error = ERROR_DISK_WRITE_PROTECTED;
	    } else
		error = ERROR_DISK_FULL;
	}
#endif
    }

error:
    MSUnLock(parentdir);

    return newlock;
}

/*
 * This routine DupLocks a file. This simply means incrementing the
 * reference count, if it was not an exclusive Lock.
 */

struct MSFileLock *
MSDupLock(fl)
struct MSFileLock *fl;
{
    if (fl == NULL)
	fl = RootLock;
    if (fl->msfl_Refcount <= 0) {
	error = ERROR_OBJECT_IN_USE;
	return NULL;
    } else {
	fl->msfl_Refcount++;
    }

    return fl;
}

/*
 * This routine DupLocks the parent of a lock, if there is one.
 */

struct MSFileLock *
MSParentDir(fl)
register struct MSFileLock *fl;
{
    if (fl == NULL || fl == RootLock) {
	error = ERROR_OBJECT_NOT_FOUND;
    } else if (fl->msfl_Parent)
	return MSDupLock(fl->msfl_Parent);

    return NULL;
}

/*
 * This routine UnLocks a file.
 */

int
MSUnLock(fl)
struct MSFileLock *fl;
{
#ifdef HDEBUG
    debug(("MSUnLock %08lx: ", fl));
    PrintDirEntry(&fl->msfl_Msd);
#endif

    if (fl) {
	if (--fl->msfl_Refcount <= 0) {
	    struct LockList *list;

	    list = (struct LockList *) fl->msfl_Node.mln_Pred;
	    Remove(fl);
	    debug(("Remove()d %08lx\n", fl));

	    /*
	     * We may need to get rid of the LockList if it is empty. This
	     * is the current LockList iff we are called from
	     * MSDiskRemoved(). Please note that we are not even sure that
	     * 'list' really is the list header, therefore the careful
	     * test if fl refers to a volume label (root lock) which is
	     * finally UnLock()ed. Because of the recursion, we only try to
	     * free the LockList iff there is no parent anymore, since
	     * otherwise list may be invalid by the time we use it.
	     */
	    if (fl->msfl_Parent) {
		MSUnLock(fl->msfl_Parent);
	    } else {
		if ((fl->msfl_Msd.msd_Attributes & ATTR_VOLUMELABEL) &&
		    ((void *) list->ll_List.mlh_Head ==
		     (void *) &list->ll_List.mlh_Tail)
		    ) {
		    FreeLockList(list);
		}
	    }
	    FreeMem(fl, (long) sizeof (*fl));
	}
    }
    return DOSTRUE;
}

/*
 * This is (among other things) the inverse of ToMSName().
 */

void
ExamineDirEntry(msd, fib)
struct MsDirEntry *msd;
register struct FileInfoBlock *fib;
{
#ifdef HDEBUG
    debug(("+ "));
    PrintDirEntry(msd);
#endif
    /*
     * Special treatment when we examine the root directory
     */
    if (msd->msd_Attributes & ATTR_VOLUMELABEL) {
	strncpy(&fib->fib_FileName[1], msd->msd_Name, 8 + 3);
	(void) ZapSpaces(&fib->fib_FileName[2], &fib->fib_FileName[1 + 8 + 3]);
    } else {
	register byte  *end,
		       *dot;

	strncpy(&fib->fib_FileName[1], msd->msd_Name, 8);
	/* Keep at least one character, even a space, before the dot */
	dot = ZapSpaces(&fib->fib_FileName[2], &fib->fib_FileName[1 + 8]);
	if (strncmp(msd->msd_Ext, "INF", 3) == 0) {
	    strcpy(dot, ".info");
	} else {
	    dot[0] = ' ';
	    strncpy(dot + 1, msd->msd_Ext, 3);
	    dot[4] = '\0';
	    end = ZapSpaces(dot, dot + 4);
	    if (end > dot)
		dot[0] = '.';
	}
    }
    fib->fib_FileName[0] = strlen(&fib->fib_FileName[1]);

    fib->fib_EntryType =
    fib->fib_DirEntryType =
	(msd->msd_Attributes & ATTR_DIR) ? FILE_DIR : FILE_FILE;
    fib->fib_Protection = 0;
    if (!(msd->msd_Attributes & ATTR_ARCHIVED))
	fib->fib_Protection |= FIBF_ARCHIVE;
    if (msd->msd_Attributes & ATTR_READONLY)
	fib->fib_Protection |= (FIBF_WRITE | FIBF_DELETE);
    if (msd->msd_Attributes & (ATTR_HIDDEN|ATTR_SYSTEM))
	fib->fib_Protection |= FIBF_HIDDEN;
    fib->fib_Size = msd->msd_Filesize;
    fib->fib_NumBlocks = (msd->msd_Filesize + Disk.bps - 1) / Disk.bps;
    ToDateStamp(&fib->fib_Date, msd->msd_Date, msd->msd_Time);
    fib->fib_Comment[0] = 0;
}

/*
 * We remember what we should do when we call ExNext with a lock on
 * a directory (enter or step over it) by a flag in fib_EntryType.
 * Unfortunately, the Commodore (1.3) List and Dir commands expect
 * that fib_EntryType contains the information that the documentation
 * (libraries/dos.h) specifies to be in fib_DirEntryType. Therefore
 * we use the low bit in fib_DiskKey instead. Yech.
 */

int
MSExamine(fl, fib)
struct MSFileLock *fl;
register struct FileInfoBlock *fib;
{
    if (fl == NULL)
	fl = RootLock;

    fib->fib_DiskKey = ((ulong) fl->msfl_DirSector << 16) |
		       fl->msfl_DirOffset +
		       1;	/* No ExNext called yet */
    ExamineDirEntry(&fl->msfl_Msd, fib);

    return DOSTRUE;
}

int
MSExNext(fl, fib)
struct MSFileLock *fl;
register struct FileInfoBlock *fib;
{
    word	    sector = fib->fib_DiskKey >> 16;
    word	    offset = (word) fib->fib_DiskKey;
    byte	   *buf;

    if (fl == NULL)
	fl = RootLock;

    if (offset & 1) {
	if (fl->msfl_Msd.msd_Attributes & ATTR_DIR) {
	    /* Enter subdirectory */
	    sector = DirClusterToSector(fl->msfl_Msd.msd_Cluster);
	    offset = 0;
	} else {
	    offset--;		/* Remember, it was odd */
	    NextDirEntry(&sector, &offset);
	}
    } else {
skip:
	NextDirEntry(&sector, &offset);
    }

    if (sector != SEC_EOF) {
	struct MsDirEntry msd;

	if (buf = GetSec(sector)) {
	    msd = *(struct MsDirEntry *) (buf + offset);
	    FreeSec(buf);
	    if (msd.msd_Name[0] == '\0') {
		goto end;
	    }
	    if (msd.msd_Name[0] & DIR_DELETED_MASK ||
		msd.msd_Name[0] == '.' ||       /* Hide "." and ".." */
		(msd.msd_Attributes & ATTR_VOLUMELABEL)) {
		goto skip;
	    }
	    OtherEndianMsd(&msd);       /* Get correct endianness */
	    fib->fib_DiskKey = ((ulong) sector << 16) | offset;
	    ExamineDirEntry(&msd, fib);

	    return DOSTRUE;
	}
    }
end:
    error = ERROR_NO_MORE_ENTRIES;
    return DOSFALSE;
}

/*
 * Convert AmigaDOS protection bits to messy attribute bits.
 */

long
MSSetProtect(parentdir, name, mask)
register struct MSFileLock *parentdir;
char	   *name;
long	   mask;
{
    register struct MSFileLock *lock;

    if (parentdir == NULL)
	parentdir = RootLock;

    lock = MSLock(parentdir, name, EXCLUSIVE_LOCK);
    if (lock) {
	/* Leave SYSTEM bit as-is */
	lock->msfl_Msd.msd_Attributes &= ATTR_SYSTEM;
	/* write or delete protected -> READONLY */
	if (mask & (FIBF_WRITE|FIBF_DELETE))
	    lock->msfl_Msd.msd_Attributes |= (ATTR_READONLY);
	/* hidden -> hidden */
	if (mask & FIBF_HIDDEN)
	    lock->msfl_Msd.msd_Attributes |= (ATTR_HIDDEN);
	/* archived=0 (default) -> archived=1 (default) */
	if (!(mask & FIBF_ARCHIVE))
	    lock->msfl_Msd.msd_Attributes |= (ATTR_ARCHIVED);
	WriteFileLock(lock);
	MSUnLock(lock);
	return TRUE;
    }

    return FALSE;
}

int
CheckLock(lock)
register struct MSFileLock *lock;
{
    register struct MSFileLock *parent;

    if (lock) {
	while (parent = lock->msfl_Parent)
	    lock = parent;
	if (lock != RootLock)
	    error = ERROR_DEVICE_NOT_MOUNTED;
    }
    return error;
}

#ifndef READONLY

void
WriteFileLock(fl)
register struct MSFileLock *fl;
{
    debug(("WriteFileLock %08lx\n", fl));

    if (fl && (int) fl->msfl_DirOffset >= 0) {
	register byte  *block = GetSec(fl->msfl_DirSector);

	if (block) {
	    CopyMem(&fl->msfl_Msd, block + fl->msfl_DirOffset,
		    (long) sizeof (fl->msfl_Msd));
	    OtherEndianMsd(block + fl->msfl_DirOffset);
	    MarkSecDirty(block);
	    FreeSec(block);
	}
    }
}

void
UpdateFileLock(fl)
register struct MSFileLock *fl;
{
    debug(("UpdateFileLock %08lx\n", fl));

    if (fl) {
	struct DateStamp dateStamp;

	DateStamp(&dateStamp);
	ToMSDate(&fl->msfl_Msd.msd_Date, &fl->msfl_Msd.msd_Time, &dateStamp);
	WriteFileLock(fl);
    }
}

#endif

struct LockList *
NewLockList(cookie)
void	       *cookie;
{
    struct LockList *ll;

    if (ll = AllocMem((long) sizeof (*ll), MEMF_PUBLIC)) {
	NewList(&ll->ll_List);
	ll->ll_Cookie = cookie;
    } else
	error = ERROR_NO_FREE_STORE;

    return ll;
}

void
FreeLockList(ll)
struct LockList *ll;
{
    debug(("FreeLockList %08lx\n", ll));

    if (ll) {
	MayFreeVolNode(ll->ll_Cookie);  /* not too happy about this */
	FreeMem(ll, (long) sizeof (*ll));
	if (ll == LockList)     /* locks on current volume */
	    LockList = NULL;
    }
}
