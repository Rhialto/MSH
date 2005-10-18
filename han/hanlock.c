/*-
 * $Id: hanlock.c,v 1.58 2005/10/19 16:53:52 Rhialto Exp $
 * $Log: hanlock.c,v $
 * Revision 1.58  2005/10/19  16:53:52  Rhialto
 * Finally a new version!
 *
 * Revision 1.56  1996/12/22  00:22:33  Rhialto
 * File locks only updated on disk when last unlocked.
 *
 * Revision 1.55  1993/12/30  23:28:00	Rhialto
 * Freeze for MAXON5.
 * Lots of small changes for LONGNAMES option.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:28:50  Rhialto
 * No real change.
 *
 * Revision 1.52  92/09/06  01:52:30  Rhialto
 * Handle /parentdir via MSParentDir().
 *
 * Revision 1.51  92/04/17  15:37:41  Rhialto
 * Freeze for MAXON.
 *
 * Revision 1.46  91/10/06  18:26:37  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.43  91/09/28  01:41:06  Rhialto
 * Changed to newer syslog stuff.
 * Fix bug that assign always refers to the rootdir.
 *
 * Revision 1.42  91/06/13  23:55:16  Rhialto
 * DICE conversion
 *
 * Revision 1.41  91/06/13  23:37:53  Rhialto
 * Fix MSSetProtect (converted dirs into files) + DICE conversion
 *
 * Revision 1.40  91/03/03  17:53:35  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.33  91/01/24  00:08:27  Rhialto
 * Fix directory-extension bug (only first sector of new cluster
 * was zeroed)
 *
 * Revision 1.32  90/11/23  23:54:18  Rhialto
 * Prepare for syslog
 *
 * Revision 1.31  90/11/10  02:48:38  Rhialto
 * Patch 3a. Introduce disk volume date. Update modification time of
 * directories.
 *
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
 * This code is (C) Copyright 1989-1997 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include "han.h"
#include "dos.h"
#include <string.h>

#if HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif
#define	debug0(x)

Prototype void NextDirEntry(sector_t *sector, int *offset);
Prototype struct DirEntry *FindNext(struct DirEntry *previous, int createit);
Prototype void PrintDirEntry(struct DirEntry *de);
Prototype struct MSFileLock *MakeLock(struct MSFileLock *parentdir, struct DirEntry *dir, ulong mode);
Prototype struct MSFileLock *MSLock(struct MSFileLock *parentdir, byte *name, ulong mode);
Prototype struct MSFileLock *MSDupLock(struct MSFileLock *fl);
Prototype struct MSFileLock *MSParentDir(struct MSFileLock *fl);
Prototype long MSUnLock(struct MSFileLock *fl);
Prototype void ExamineDirEntry(struct MsDirEntry *msd, struct FileInfoBlock *fib, int checksum);
Prototype long MSExamine(struct MSFileLock *fl, struct FileInfoBlock *fib);
Prototype long MSExNext(struct MSFileLock *fl, struct FileInfoBlock *fib);
Prototype long ExamineDirEntries(sector_t sector, int offset, struct FileInfoBlock *fib);
Prototype long MSSetProtect(struct MSFileLock *parentdir, char *name, long mask);
Prototype int CheckLock(struct MSFileLock *lock);
Prototype void WriteDirtyFileLock(struct MSFileLock *fl);
Prototype void WriteFileLock(struct MSFileLock *fl);
Prototype void DirtyFileLock(struct MSFileLock *fl);
Prototype void UpdateFileLock(struct MSFileLock *fl);
Prototype struct LockList *NewLockList(void *cookie);
Prototype void FreeLockList(struct LockList *ll);

Prototype struct LockList *LockList;
Prototype struct MSFileLock *RootLock;
Prototype struct MSFileLock *EmptyFileLock;
Prototype const struct DirEntry FakeRootDirEntry;
Prototype const byte DotDot[1 + L_8 + L_3];

struct LockList *LockList;	/* List of all locked files we have. Note
				 * this is not the same as all locks we
				 * have */
struct MSFileLock *RootLock;	/* Lock on root directory */
struct MSFileLock *EmptyFileLock;	/* 2nd result of MSLock() */

#if LONGNAMES
const struct DirEntry FakeRootDirEntry = {
    {				/* de_Msd */
	0,			/* msd_Time */
	DATE_MIN,		/* msd_Date, 1/1/80 */
	0,			/* msd_Cluster */
	0,			/* msd_Filesize */
	ATTR_VOLUMELABEL,	/* msd_Attributes */
	"Unnamed              " /* msd_Name */
    },
    ROOT_SEC,			/* de_Sector */
    -2,				/* de_Offset */
#if VFATSUPPORT
    0,				/* de_VfatnameSector */
    0				/* de_VfatnameOffset */
#endif
};
#else
const struct DirEntry FakeRootDirEntry = {
    {				/* de_Msd */
	"Unnamed ",             /* msd_Name */
	"   ",                  /* msd_Ext */
	ATTR_VOLUMELABEL,	/* msd_Attributes */
	0,			/* msd_Case */
	0,			/* msd_CreationTimems */
	0,			/* msd_CreationTime */
	DATE_MIN,		/* msd_CreationDate, 1/1/80 */
	0,			/* msd_AccessTime */
	DATE_MIN,		/* msd_AccessDate, 1/1/80 */
	0,			/* msd_ModTime */
	DATE_MIN,		/* msd_ModDate, 1/1/80 */
	0,			/* msd_Cluster */
	0			/* msd_Filesize */
    },
    ROOT_SEC,			/* de_Sector */
    -2				/* de_Offset */
#if VFATSUPPORT
    ,0,				/* de_VfatnameSector */
    0				/* de_VfatnameOffset */
#endif
};
#endif
const byte DotDot[1 + L_8 + L_3] = "..          ";

Prototype sector_t NextDirSector(sector_t sector);

sector_t
NextDirSector(sector_t sector)
{
    if (sector >= Disk.datablock) {
	/* Must be subdirectory */
	sector = NextClusteredSector(sector);
	debug(("NextClusteredSector: %ld\n", (long)sector));
    } else {
	if (++sector >= Disk.datablock) {
	    sector = SEC_EOF;
	}
    }
    return sector;
}

void
NextDirEntry(sector, offset)
sector_t  *sector;
int	  *offset;
{
    if ((*offset += MS_DIRENTSIZE) >= Disk.bps) {
	*offset = 0;
	*sector = NextDirSector(*sector);
    }
    /* else no more work needed */
}

#if VFATSUPPORT
Prototype void PrevDirEntry(sector_t  *sector, int *offset);

void
PrevDirEntry(
    sector_t  *sector,
    int	      *offset)
{
    if ((int)(*offset -= MS_DIRENTSIZE) < 0) {
	*offset = Disk.bps - MS_DIRENTSIZE;
	*sector = PrevDirSector(*sector);
    }
    /* else no more work needed */
}

#endif

/*
 * Get the directory entry following the given one. If requested, we make
 * the directory longer.
 */

struct DirEntry *
FindNext(previous, createit)
struct DirEntry *previous;
int		createit;
{
    byte	   *sector;
    sector_t	    prevsec = previous->de_Sector;

    NextDirEntry(&previous->de_Sector, &previous->de_Offset);

    if (previous->de_Sector == SEC_EOF) {
	error = ERROR_OBJECT_NOT_FOUND;
#if ! READONLY
	if (createit) {
	    int 	    clearblocks;

	    if (prevsec < Disk.datablock - 1) { /* Should not be necessary */
		previous->de_Sector = prevsec + 1;
		clearblocks = 1;	/* Clear just one root dir block */
	    } else if (prevsec >= Disk.datablock) {
		previous->de_Sector = FindFreeSector(prevsec);
		clearblocks = Disk.spc; /* Clear the entire (implied) cluster */
	    }
	    if (previous->de_Sector != SEC_EOF) {
		for (prevsec = previous->de_Sector; clearblocks; clearblocks--) {
		    sector = EmptySec(prevsec);
		    memset(sector, 0, (int) Disk.bps);
		    MarkSecDirty(sector);
		    FreeSec(sector);
		    prevsec++;
		}

		/*
		 * Just return a clear directory entry
		 */
		setmem(&previous->de_Msd, sizeof (previous->de_Msd), 0);

		return previous;
	    }
	}
#endif	/* ! READONLY */
    } else if (sector = ReadSec(previous->de_Sector)) {
	previous->de_Msd = *(struct MsDirEntry *)(sector + previous->de_Offset);
	FreeSec(sector);

	return previous;
    }
    return NULL;
}

#if HDEBUG

void
PrintDirEntry(de)
struct DirEntry *de;
{
#if VFATSUPPORT
    debug(("%ld,%ld VFat(%ld,%ld) ", (long)de->de_Sector, (long)de->de_Offset,
	(long)de->de_VfatnameSector, (long)de->de_VfatnameOffset));
#else
    debug(("%ld,%ld ", (long)de->de_Sector, (long)de->de_Offset));
#endif
#if LONGNAMES
    debug(("%.21s attr:%lx time:%lx date:%lx start:%lx size:%lx\n",
	   de->de_Msd.msd_Name,
	   (long)de->de_Msd.msd_Attributes,
	   (long)de->de_Msd.msd_ModTime,
	   (long)de->de_Msd.msd_ModDate,
	   (long)de->de_Msd.msd_Cluster,
	   (long)de->de_Msd.msd_Filesize
	   ));
#else
    debug(("%.8s.%.3s attr:%lx time:%lx date:%lx start:%lx size:%lx\n",
	   de->de_Msd.msd_Name,
	   de->de_Msd.msd_Ext,
	   (long)de->de_Msd.msd_Attributes,
	   (long)de->de_Msd.msd_ModTime,
	   (long)de->de_Msd.msd_ModDate,
	   (long)de->de_Msd.msd_Cluster,
	   (long)de->de_Msd.msd_Filesize
	   ));
#endif	/* LONGNAMES */
}

#endif	/* HDEBUG */

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
    struct MSFileLock *fl;
    struct MSFileLock *nextfl;

    if (mode != EXCLUSIVE_LOCK || (dir->de_Msd.msd_Attributes & ATTR_DIR))
	mode = SHARED_LOCK;

#if HDEBUG
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
#if HDEBUG
	debug(("> "));
	PrintDirEntry((struct DirEntry *)&fl->msfl_Msd);
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
    fl->msfl_Parent = (parentdir != NULL) ? MSDupLock(parentdir) :
					    (struct MSFileLock *) NULL;

    fl->msfl_Refcount = (mode == EXCLUSIVE_LOCK) ? -1 : 1;
    fl->msfl_DirSector = dir->de_Sector;
    fl->msfl_DirOffset = dir->de_Offset;
#if VFATSUPPORT
    fl->msfl_VfatnameSector = dir->de_VfatnameSector;
    fl->msfl_VfatnameOffset = dir->de_VfatnameOffset;
#endif
    fl->msfl_Msd = dir->de_Msd;

    AddHead((struct List *)&LockList->ll_List, (struct Node *)fl);

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
    struct DirEntry *de;
    struct DirEntry sde;
    byte	   *nextpart;
    byte	    component[L_8 + L_3 + 1];	/* Note:
						 * maybe not nul-terminated */
    int 	    createit = 0;
#if VFATSUPPORT
    int		    longcomponentlength;
    int             checksum;
    int		    olddos;
#endif
    sector_t	    slotsec;		/* First free slot location */
    int		    slotoff;
    int		    wincount;		/* how many slots needed */
    int		    slotcount;		/* how many slots found */

    newlock = NULL;

    /*
     * See if we have an absolute path name (starting at the root).
     */
    {
	byte  *colon;

	if (colon = strchr(name, ':')) {
	    debug(("name == %lx \"%s\"\n", name, name));
	    name = colon + 1;
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
#if HDEBUG
    if (!parentdir)
	debug(("Parentdir == NULL\n"));
#endif
    parentdir = MSDupLock(parentdir);

    /*
     * Start with the directory entry of the parent dir.
     */

newdir0:
    sde.de_Msd = parentdir->msfl_Msd;
    sde.de_Sector = parentdir->msfl_DirSector;
    sde.de_Offset = parentdir->msfl_DirOffset;
#if HDEBUG
    debug(("pdir %08lx: ", parentdir));
    PrintDirEntry((struct DirEntry *)&parentdir->msfl_Msd);
#endif

newdir:
#if VFATSUPPORT
    olddos = 1;
    checksum = -1;
#endif

    if (name[0] == '/') {       /* Parentdir */
	name++;
	if (newlock = MSParentDir(parentdir)) {
	    MSUnLock(parentdir);
	    parentdir = newlock;
	    goto newdir0;
	}
	error = ERROR_OBJECT_NOT_FOUND;
	goto some_error;
    } else if (name[0] == '\0') {
	/*
	 * Are we at the end of the name? Then we are finished now. This
	 * works because sde contains the directory entry of parentdir.
	 */
	goto exit;
    } else {			/* Filename or directory part */
#if VFATSUPPORT
	nextpart = ComponentEnd(name);
	longcomponentlength = nextpart - name;

	switch (ami2dosfn(name, component, longcomponentlength, 
		(Interleave & OPT_SHORTNAME)? -1 : 0)) {
	case U2D_CANNOT_CONVERT:
	    error = ERROR_INVALID_COMPONENT_NAME;
	    goto some_error;
	case U2D_CONVERTED_SAME:
	case U2D_CONVERTED_TRUNC:
	    wincount = 1;
	    break;
	case U2D_CONVERTED_OK:
	    wincount = winSlotCnt(name, longcomponentlength) + 1;
	    break;
	case U2D_CONVERTED_GENNR:
	    olddos = 0;	/* don't match old-style short name entries */
	    wincount = winSlotCnt(name, longcomponentlength) + 1;
	    break;
	}

	if (Interleave & OPT_SHORTNAME) {
	    /* Just in case U2D_CONVERTED_OK was returned */
	    wincount = 1;
	}

	/*
	 * Suppress search for slots unless creating
	 * file and at end of pathname, in which case
	 * we watch for a place to put the new file in
	 * case it doesn't already exist.
	 */
	slotcount = wincount;

#else
	nextpart = ToMSName(component, name);
#endif
	debug(("Component: '%.11s'\n", component));
	if (nextpart[0] != '/') {
	    nextpart = NULL;
#if ! READONLY
	    /*
	     * See if we are requested to get an empty spot in the directory
	     * if the given name does not exist already. The value of mode is
	     * not important until we actually create the filelock.
	     */
	    if (!(mode & MODE_CREATEFILE)) {
		mode ^= MODE_CREATEFILE;
		createit = 1;
		slotcount = 0;
#if VFATSUPPORT
		if (!(Interleave & OPT_SHORTNAME))
		    ToUniqueMSName(component, parentdir, name,
							longcomponentlength);
		/* XXX error checking */
#endif
	    }
#endif	/* ! READONLY */
	} else
	    nextpart++; 	/* Skip over '/' */
    }

    /*
     * If there is more name, we enter the directory, and here we get the
     * first entry.
     */

    sde.de_Sector = DirClusterToSector(sde.de_Msd.msd_Cluster);
    sde.de_Offset = 0;
#if VFATSUPPORT
    sde.de_VfatnameSector = 0;
    sde.de_VfatnameOffset = 0;
#endif

    if ((sector = ReadSec(sde.de_Sector)) == NULL)
	goto some_error;

    sde.de_Msd = *(struct MsDirEntry *)sector;
    FreeSec(sector);

    for (de = &sde; de; de = FindNext(&sde, createit)) {
	if (sde.de_Msd.msd_Name[0] == DIR_DELETED ||
	    sde.de_Msd.msd_Name[0] == DIR_END) {
	    /* Drop memory of previous long matches */
#if VFATSUPPORT
	    checksum = -1;
	    debug0(("DIR_DELETED: checksum now %d\n", checksum));

#endif
	    if (slotcount < wincount) {
#if 1 || VFATSUPPORT
		if (!slotcount) {
		    /* If first free slot, remember where */
		    slotsec = sde.de_Sector;
		    slotoff = sde.de_Offset;
		}
#endif
		slotcount++;
	    }
	    if (sde.de_Msd.msd_Name[0] == DIR_END) {
		error = ERROR_OBJECT_NOT_FOUND;
		de = NULL;
		break;
	    }
	} else {
	    /* If there wasn't enough space for our subentries,
	     * forget about the empty space.
	     */
	    if (slotcount < wincount)
		slotcount = 0;
#if VFATSUPPORT
	    /* Check for VFAT long name */
	    if (sde.de_Msd.msd_Attributes == ATTR_WIN95) {
		if (Interleave & OPT_SHORTNAME)
		    continue;
		/* XXX test SE_LAST bit to detect long names when only
		 * short name given? */
		if (checksum == -1) {
		    /* If we're not matching a long name yet,
		     * remember where we are. This might be
		     * the beginning of our name. OTOH, it might
		     * not be, so we need to recheck later on.
		     */
		    sde.de_VfatnameSector = sde.de_Sector;
		    sde.de_VfatnameOffset = sde.de_Offset;
		}
		checksum = CheckVfatSubentry(name, longcomponentlength,
		    (struct MsVfatSubEntry *) &sde.de_Msd, checksum);
		debug0(("ATTR_WIN95: checksum now %d\n", checksum));
		continue;
	    }
#endif
	    if (sde.de_Msd.msd_Attributes & ATTR_VOLUMELABEL) {
#if VFATSUPPORT
		checksum = -1;
		debug(("ATTR_VOLUMELABEL: checksum now %d\n", checksum));
#endif
		continue;
	    }
#if VFATSUPPORT
	    if (checksum != VfatChecksum(sde.de_Msd.msd_Name) &&
		(!olddos ||
		 memcmp(sde.de_Msd.msd_Name, component, L_8 + L_3) != 0)) {
		checksum = -1;
		continue;
	    }
#else
	    if (memcmp(sde.de_Msd.msd_Name, component, L_8 + L_3) != 0) {
		continue;
	    }
#endif

	    /* If we get here, the name matches */
	    OtherEndianMsd(&sde.de_Msd);
#if VFATSUPPORT
	    if (checksum == -1) {
		sde.de_VfatnameSector = 0;
		sde.de_VfatnameOffset = 0;
	    }
#endif

	    if (sde.de_Msd.msd_Attributes & ATTR_DIRECTORY) {
		/* It is a directory */
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
	    } else {
		/* It is a file */
		if (nextpart) {
		    error = ERROR_OBJECT_WRONG_TYPE;
		    de = NULL;
		}
	    }
	    break;
	}
    }

exit:
    if (de) {
	newlock = MakeLock(parentdir, &sde, mode);
    } else {
	newlock = NULL;
#if ! READONLY
	if (createit && 	/* Do we want to make it? */
	    error == ERROR_OBJECT_NOT_FOUND &&	/* does it not exist yet? */
	    nextpart == NULL) { /* do we have the last part of the name */
	    if (IDDiskState == ID_VALIDATED) {
#if VFATSUPPORT
#if 0		/* This should not be necessary, since FindNext()
		 * already makes one empty slot, which should be counted
		 * above.
		 */
		if (!slotcount) {
		    slotcount = 1;
		    slotsec = sde.de_Sector;
		    slotoff = sde.de_Offset;
		}
#endif
		if (slotcount < wincount) {
		    debug(("wincount %d > slotcount %d\n", wincount, slotcount));
		    /* need to create some room at the end of the dir */
		    do {
			de = FindNext(&sde, 1);
			debug(("new slot at %ld,%ld\n",
				(long)sde.de_Sector, (long)sde.de_Offset));
			slotcount++;
		    } while (de && slotcount < wincount);
		}
#endif
		if (slotcount > 0) {	/* is there any room? */
		    error = 0;
		    setmem(&sde.de_Msd, sizeof (sde.de_Msd), 0);
		    memcpy(sde.de_Msd.msd_Name, component, L_8 + L_3);
#if VFATSUPPORT
		    sde.de_VfatnameSector = slotsec;
		    sde.de_VfatnameOffset = slotoff;
		    if (slotcount < wincount)
			slotcount = 1;
		    WriteLongName(&sde, slotcount - 1, name,
				    longcomponentlength);
		    debug(("WriteLongName: longname: %ld,%ld  shortname: %ld,%ld\n",
			    slotsec, (long)slotoff,
			    sde.de_Sector, (long)sde.de_Offset));
#else
		    sde.de_Sector = slotsec;
		    sde.de_Offset = slotoff;
#endif
		    EmptyFileLock = MakeLock(parentdir, &sde, mode);
		    WriteFileLock(EmptyFileLock);
		} else
		    error = ERROR_DISK_FULL;
	    } else
		error = ERROR_DISK_WRITE_PROTECTED;
	}
#endif	/* ! READONLY */
    }

some_error:
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
struct MSFileLock *fl;
{
    if (fl == NULL) {
	error = ERROR_OBJECT_NOT_FOUND;
    } else if (fl == RootLock) {
	/* Do nothing: return NULL */
    } else if (fl->msfl_Parent)
	return MSDupLock(fl->msfl_Parent);

    return NULL;
}

/*
 * This routine UnLocks a file.
 */

long
MSUnLock(fl)
struct MSFileLock *fl;
{
#if HDEBUG
    debug(("MSUnLock %08lx: ", fl));
    PrintDirEntry((struct DirEntry *)&fl->msfl_Msd);
#endif

    if (fl) {
	if (--fl->msfl_Refcount <= 0) {
	    struct LockList *list;

	    if (fl->msfl_Flags & MSFL_DIRTY) {
		WriteFileLock(fl);
	    }

	    list = (struct LockList *) fl->msfl_Node.mln_Pred;
	    Remove((struct Node *)fl);
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
		if (((fl->msfl_Msd.msd_Attributes & ~ATTR_ARCHIVE) ==
			ATTR_VOLUMELABEL) &&
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

void
CopyShortName(
    byte *dest,
    byte *source)
{
#if VFATSUPPORT
    dest[0] = dos2amifn(source, dest + 1, 0);
#else
#if LONGNAMES
	memcpy(&dest[1], source, L_8);
	(void) ZapSpaces(&dest[2], &dest[1 + L_8]);
#else
	byte	       *end,
		       *dot;

	memcpy(&dest[1], source, L_8);
	/* Keep at least one character, even a space, before the dot */
	dot = ZapSpaces(&dest[2], &dest[1 + L_8]);
	if (memcmp(&source[L_8], "INF", L_3) == 0) {
	    strcpy(dot, ".info");
	} else {
	    dot[0] = ' ';
	    memcpy(dot + 1, &source[L_8], L_3);
	    end = ZapSpaces(dot, dot + 1 + L_3);
	    if (end > dot)
		dot[0] = '.';
	}
#endif	/* LONGNAMES */
    dest[0] = strlen(&dest[1]);
#endif
}

/*
 * This is (among other things) the inverse of ToMSName().
 */

void
ExamineDirEntry(
    struct MsDirEntry *msd,
    struct FileInfoBlock *fib,
    int checksum)
{
#if HDEBUG
    debug(("+ "));
    PrintDirEntry((struct DirEntry *)msd);
#endif
    fib->fib_Comment[0] = 0;
    /*
     * Special treatment when we examine the root directory
     */
    if (msd->msd_Attributes == ATTR_VOLUMELABEL) {
	memcpy(&fib->fib_FileName[1], msd->msd_Name, L_8 + L_3);
	(void) ZapSpaces(&fib->fib_FileName[2], &fib->fib_FileName[1 + L_8 + L_3]);
	fib->fib_FileName[0] = strlen(&fib->fib_FileName[1]);
#if VFATSUPPORT
    } else if (checksum != -1 && checksum == VfatChecksum(msd->msd_Name)) {
	/* use collected long vfat name ... */
	fib->fib_FileName[0] = strlen(&fib->fib_FileName[1]);

	/* give short name as comment */
	CopyShortName(fib->fib_Comment, msd->msd_Name);
#if 0
	/*
	 * XXX hack to show both long and short name in dir...
	 * by stepping back DiskKey, the next ExNext will end up
	 * at this entry again, but without seeing the long name.
	 * Therefore this will not loop.
	 */
	{
	    sector_t sector = UNPACK_EXAMINE_SECTOR(fib->fib_DiskKey);
	    int offset = UNPACK_EXAMINE_OFFSET(fib->fib_DiskKey);
	    fib->fib_DiskKey =
		PACK_EXAMINE_LOCATION(sector, offset - MS_DIRENTSIZE);
	}
#endif
#endif
    } else {
	CopyShortName(fib->fib_FileName, msd->msd_Name);
    }

    fib->fib_EntryType =
    fib->fib_DirEntryType =
	(msd->msd_Attributes & ATTR_DIR) ? FILE_DIR : FILE_FILE;
    fib->fib_Protection = 0;
    if (!(msd->msd_Attributes & ATTR_ARCHIVE))
	fib->fib_Protection |= FIBF_ARCHIVE;
    if (msd->msd_Attributes & ATTR_READONLY)
	fib->fib_Protection |= (FIBF_WRITE | FIBF_DELETE);
#if defined(FIBF_HIDDEN)
    if (msd->msd_Attributes & (ATTR_HIDDEN|ATTR_SYSTEM))
	fib->fib_Protection |= FIBF_HIDDEN;
#endif
    fib->fib_Size = msd->msd_Filesize;
    fib->fib_NumBlocks = (msd->msd_Filesize + Disk.bps - 1) / Disk.bps;
    ToDateStamp(&fib->fib_Date, msd->msd_ModDate, msd->msd_ModTime);
}

/*
 * We remember what we should do when we call ExNext with a lock on
 * a directory (enter or step over it) by a flag in fib_EntryType.
 * Unfortunately, the Commodore (1.3) List and Dir commands expect
 * that fib_EntryType contains the information that the documentation
 * (libraries/dos.h) specifies to be in fib_DirEntryType. Therefore
 * we use the low bit in fib_DiskKey instead. Yech.
 */

long
MSExamine(fl, fib)
struct MSFileLock *fl;
struct FileInfoBlock *fib;
{
    if (fl == NULL)
	fl = RootLock;

#if VFATSUPPORT
    if (fl->msfl_VfatnameSector && !CheckLock(fl)) {
	/* XXX uses on-disk copy of file lock... should use in-memory copy? */
	debug(("MSExamine: Using long file name...\n"));
	long result = ExamineDirEntries(fl->msfl_VfatnameSector,
	    fl->msfl_VfatnameOffset, fib);
	fib->fib_DiskKey |= 1;	/* No ExNext called yet */
	if (result)
	    return result;
	/* retry with short name; undo the error */
	error = 0;
    }
#endif
    fib->fib_DiskKey =
	PACK_EXAMINE_LOCATION(fl->msfl_DirSector, fl->msfl_DirOffset) | 
	   1;			/* No ExNext called yet */
    ExamineDirEntry(&fl->msfl_Msd, fib, -1);

    return DOSTRUE;
}

long
MSExNext(fl, fib)
struct MSFileLock *fl;
struct FileInfoBlock *fib;
{
    sector_t	    sector = UNPACK_EXAMINE_SECTOR(fib->fib_DiskKey);
    int		    offset = UNPACK_EXAMINE_OFFSET(fib->fib_DiskKey);

    if (fl == NULL)
	fl = RootLock;

    if (fib->fib_DiskKey & 1) {
	if (fl->msfl_Msd.msd_Attributes & ATTR_DIR) {
	    /* Enter subdirectory */
	    sector = DirClusterToSector(fl->msfl_Msd.msd_Cluster);
	    offset = 0;
	} else {
	    /* offset--;		/ * Remember, it was odd */
	    NextDirEntry(&sector, &offset);
	}
    } else {
	NextDirEntry(&sector, &offset);
    }

    return ExamineDirEntries(sector, offset, fib);
}

long
ExamineDirEntries(sector_t sector, int offset, struct FileInfoBlock *fib)
{
    int             checksum = -1;
#if VFATSUPPORT
    int		    count;
#endif

    for (; sector != SEC_EOF; NextDirEntry(&sector, &offset)) {
	byte           *buf;

	if (buf = ReadSec(sector)) {
	    struct MsDirEntry msd;

	    msd = *(struct MsDirEntry *) (buf + offset);
	    FreeSec(buf);
	    if (msd.msd_Name[0] == DIR_END)
		break;
	    if (msd.msd_Name[0] == DIR_DELETED) {
		checksum = -1;
		continue;
	    }
#if VFATSUPPORT
	    if ((msd.msd_Attributes == ATTR_WIN95)) {
		if (!(Interleave & OPT_SHORTNAME)) {
		    struct MsVfatSubEntry *se = (struct MsVfatSubEntry *)&msd;
		    count = se->se_Count;
		    checksum = ExamineVfatSubEntry(se, fib, checksum);
		}
		continue;
	    }
#endif
	    if (msd.msd_Name[0] == '.' ||       /* Hide "." and ".." */
		(msd.msd_Attributes & ATTR_VOLUMELABEL)) {
		checksum = -1;
		continue;
	    }
#if VFATSUPPORT
	    /*
	     * Simplistic attempt to weed out incomplete entries.
	     * Should really test if all subentries were seen.
	     */
	    if (checksum != -1 && (count & SE_COUNT) != 1)
		checksum = -1;
#endif
	    OtherEndianMsd(&msd);	/* Get correct endianness */
	    fib->fib_DiskKey = PACK_EXAMINE_LOCATION(sector, offset);
	    ExamineDirEntry(&msd, fib, checksum);

	    return DOSTRUE;
	} else
	    break;
    }

    error = ERROR_NO_MORE_ENTRIES;
    return DOSFALSE;
}

/*
 * Convert AmigaDOS protection bits to messy attribute bits.
 */

long
MSSetProtect(parentdir, name, mask)
struct MSFileLock *parentdir;
char	   *name;
long	   mask;
{
    struct MSFileLock *lock;

    if (parentdir == NULL)
	parentdir = RootLock;

    lock = MSLock(parentdir, name, EXCLUSIVE_LOCK);
    if (lock) {
	/* Leave other bits as they are */
	lock->msfl_Msd.msd_Attributes &=
	    ~(ATTR_READONLY | ATTR_HIDDEN | ATTR_ARCHIVE);
	/* write or delete protected -> READONLY */
	if (mask & (FIBF_WRITE|FIBF_DELETE))
	    lock->msfl_Msd.msd_Attributes |= ATTR_READONLY;
#if defined(FIBF_HIDDEN)
	/* hidden -> hidden */
	if (mask & FIBF_HIDDEN)
	    lock->msfl_Msd.msd_Attributes |= ATTR_HIDDEN;
#endif
	/* archive=0 (default) -> archived=1 (default) */
	if (!(mask & FIBF_ARCHIVE))
	    lock->msfl_Msd.msd_Attributes |= ATTR_ARCHIVE;
	WriteFileLock(lock);
	MSUnLock(lock);
	return DOSTRUE;
    }

    return DOSFALSE;
}

int
CheckLock(lock)
struct MSFileLock *lock;
{
    struct MSFileLock *parent;

    if (lock) {
	while (parent = lock->msfl_Parent)
	    lock = parent;
	if (lock != RootLock)
	    error = ERROR_DEVICE_NOT_MOUNTED;
    }
    return error;
}

#if ! READONLY

void
WriteDirtyFileLock(fl)
struct MSFileLock *fl;
{
    debug(("WriteDirtyFileLock %08lx\n", fl));

    if (fl && (fl->msfl_Flags & MSFL_DIRTY)) {
	WriteFileLock(fl);
    }
}

void
WriteFileLock(fl)
struct MSFileLock *fl;
{
    debug(("WriteFileLock %08lx\n", fl));

    if (fl && (short) fl->msfl_DirOffset >= 0) {
	byte  *block = ReadSec(fl->msfl_DirSector);

	if (block) {
	    *(struct MsDirEntry *)(block + fl->msfl_DirOffset) = fl->msfl_Msd;
	    OtherEndianMsd((struct MsDirEntry *)(block + fl->msfl_DirOffset));
	    MarkSecDirty(block);
	    FreeSec(block);
	    fl->msfl_Flags &= ~MSFL_DIRTY;
	}
    }
}

void
UpdateFileLock(fl)
struct MSFileLock *fl;
{
    debug(("UpdateFileLock %08lx\n", fl));

    if (fl) {
	struct DateStamp dateStamp;

	DateStamp(&dateStamp);
	ToMSDate(&fl->msfl_Msd.msd_ModDate, &fl->msfl_Msd.msd_ModTime, &dateStamp);
	fl->msfl_Flags |= MSFL_DIRTY;
    }
}

void
DirtyFileLock(fl)
struct MSFileLock *fl;
{
    debug(("DirtyFileLock %08lx\n", fl));

    if (fl) {
	fl->msfl_Flags |= MSFL_DIRTY;
    }
}

#endif	/* !READONLY */

struct LockList *
NewLockList(cookie)
void	       *cookie;
{
    struct LockList *ll;

    if (ll = AllocMem((long) sizeof (*ll), MEMF_PUBLIC)) {
	NewList((struct List *)&ll->ll_List);
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
	MayFreeVolNode(ll->ll_Cookie);	/* not too happy about this */
	FreeMem(ll, (long) sizeof (*ll));
	if (ll == LockList)	/* locks on current volume */
	    LockList = NULL;
    }
}
