/*-
 * $Id: hanfile.c,v 1.54 1993/06/24 05:12:49 Rhialto Exp $
 * $Log: hanfile.c,v $
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * MSCreateDir gave you an exclusive lock contrary to our liberal policy.
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:29:23  Rhialto
 * Default conversion settable.
 * OFFSET_END seeks were done in the wrong direction ARGH!
 * Protect seek pos from being past EOF (due to SetFileSize).
 * Count free clusters instead of free sectors.
 *
 * Revision 1.51  92/04/17  15:37:53  Rhialto
 * Freeze for MAXON.
 *
 * Revision 1.46  91/10/06  18:24:36  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.45  91/10/03  23:36:16  Rhialto
 * Implement in-situ conversions during Read()/Write()
 *
 * Revision 1.43  91/09/28  01:45:39  Rhialto
 * Changed to newer syslog stuff.
 *
 * Revision 1.42  91/06/13  23:56:14  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:53:53  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.32  90/11/23  23:54:57  Rhialto
 * Prepare for syslog
 *
 * Revision 1.31  90/11/10  02:50:04  Rhialto
 * Patch 3a. Update modification time of directories.
 *
 * Revision 1.30  90/06/04  23:17:33  Rhialto
 * Release 1 Patch 3
 *
 * HANFILE.C
 *
 * The code for the messydos file system handler.
 *
 * This parts handles files and the File Allocation Table.
 *
 * This code is (C) Copyright 1989-1993 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include "han.h"
#include "dos.h"
#include <string.h>

#ifdef CONVERSIONS
#   include "hanconv.h"
#endif
#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

Prototype int GetFat(void);
Prototype void FreeFat(void);
Prototype word GetFatEntry(word cluster);
Prototype void SetFatEntry(word cluster, word value);
Prototype word FindFreeCluster(word prev);
Prototype word ExtendClusterChain(word cluster);
Prototype void FreeClusterChain(word cluster);
Prototype struct MSFileHandle *MakeMSFileHandle(struct MSFileLock *fl, long mode);
Prototype struct MSFileHandle *MSOpen(struct MSFileLock *parentdir, char *name, long mode);
Prototype long MSClose(struct MSFileHandle *fh);
Prototype long FilePos(struct MSFileHandle *fh, long position, long mode);
Prototype void AdjustSeekPos(struct MSFileHandle *fh);
Prototype long MSSeek(struct MSFileHandle *fh, long position, long mode);
Prototype long MSRead(struct MSFileHandle *fh, byte *userbuffer, long size);
Prototype long MSWrite(struct MSFileHandle *fh, byte *userbuffer, long size);
Prototype long MSDeleteFile(struct MSFileLock *parentdir, byte *name);
Prototype long MSSetDate(struct MSFileLock *parentdir, byte *name, struct DateStamp *datestamp);
Prototype struct MSFileLock *MSCreateDir(struct MSFileLock *parentdir, byte *name);
Prototype long MSRename(struct MSFileLock *slock, byte *sname, struct MSFileLock *dlock, byte *dname);

/*
 * Read the FAT from the disk, and count the free clusters.
 */

int
GetFat()
{
    int 	    i;
    byte	   *secptr;

    if (!Fat && !(Fat = AllocMem((long) Disk.bps * Disk.spf, BufMemType))) {
	debug(("No memory for FAT\n"));
	return 0;
    }
    FatDirty = FALSE;
    for (i = 0; i < Disk.spf; i++) {
	if (secptr = ReadSec(Disk.res + i)) {
	    CopyMem(secptr, Fat + i * Disk.bps, (long) Disk.bps);
	    FreeSec(secptr);
	} else {
	    /* q&d way to set the entire FAT to FAT_EOF */
	    setmem(Fat + i * Disk.bps, (int) Disk.bps, FAT_EOF);        /* 0xFF */
	}
    }

    debug(("counting free clusters\n"));

    Disk.freeclusts = 0;
    for (i = MS_FIRSTCLUST; i <= Disk.maxclst; i++) {
	if (GetFatEntry((word) i) == FAT_UNUSED)
	    Disk.freeclusts++;
    }

    return 1;
}

void
FreeFat()
{
    if (Fat) {
	FreeMem(Fat, (long) Disk.bps * Disk.spf);
	Fat = NULL;
	FatDirty = FALSE;
    }
}

/*-
 *  The FAT consists of 12-bits entries for each cluster,
 *  indicating the next cluster in the chain, or FFF for EOF.
 *
 *  Every two entries are packed in three bytes, like this:
 *
 *  Two entries 	abc  123 (for one cluster and the next)
 *  are packed as	bc 3a 12
-*/

word
GetFatEntry(cluster)
word		cluster;
{
    if (!Fat && !GetFat())
	return FAT_EOF;

    if (Disk.fat16bits) {
	return OtherEndianWord(((word *)Fat)[cluster]);
    } else {
	register int	offset = 3 * (cluster / 2);
	register word	twoentries;

	if (cluster & 1) {
	    twoentries = Fat[offset + 1] >> 4;
	    twoentries |= Fat[offset + 2] << 4;
	} else {
	    twoentries = Fat[offset];
	    twoentries |= (Fat[offset + 1] & 0x0F) << 8;
	}

	/*
	 * Convert the special values 0xFF7 .. 0xFFF to 16 bits so they
	 * can be checked consistently.
	 */
	if (twoentries >= 0xFF7)
	    twoentries |= 0xF000;

	return twoentries;
    }
}

#ifndef READONLY

void
SetFatEntry(cluster, value)
word		cluster;
word		value;
{
    if (!Fat && !GetFat())
	return;

    if (Disk.fat16bits) {
	((word *)Fat)[cluster] = OtherEndianWord(value);
    } else {
	register int	offset = 3 * (cluster / 2);

	if (cluster & 1) {          /* 123 kind of entry */
	    Fat[offset + 2] = value >> 4;
	    Fat[offset + 1] &= 0x0F;
	    Fat[offset + 1] |= (value & 0x0F) << 4;
	} else {		    /* abc kind of entry */
	    Fat[offset + 0] = value;
	    Fat[offset + 1] &= 0xF0;
	    Fat[offset + 1] |= (value >> 8) & 0x0F;
	}
    }

    FatDirty = TRUE;
    StartTimer(4);
}

/*
 * Find a free cluster to install as the one following this one. Start
 * looking for it right after the given one, so we allocate the cluster
 * chain as contiguous as possible. If we run off the end of the disk, we
 * start again at the beginning. The termination test should not be
 * necessary (and won't work if we are given MSFIRSTCLUST - 1) but won't
 * harm either.
 */

word
FindFreeCluster(prev)
word		prev;
{
    register word   i;

    if (prev == 0 || prev == FAT_EOF)
	prev = MS_FIRSTCLUST - 1;

    if (Disk.freeclusts > 0) {
	for (i = prev + 1; i != prev; i++) {
	    if (i > Disk.maxclst)       /* Wrap around */
		i = MS_FIRSTCLUST;
	    if (GetFatEntry(i) == FAT_UNUSED) {
		SetFatEntry(i, FAT_EOF);
		if (prev >= MS_FIRSTCLUST)
		    SetFatEntry(prev, i);
		Disk.freeclusts--;
		return i;
	    }
	}
    }
    return FAT_EOF;
}

/*
 * Add a cluster to a cluster chain. For input, we get some cluster we
 * know that is on the chain, even if it is the first one.
 */

word
ExtendClusterChain(cluster)
register word	cluster;
{
    register word   nextcluster;

    /*
     * Find the end of the cluster chain to tack the new cluster on to.
     * Then FindFreeCluster will (or won't) extend the chain for us.
     */
    if (cluster != 0) {
	while ((nextcluster = NextCluster(cluster)) != FAT_EOF) {
	    cluster = nextcluster;
	}
    }

    return FindFreeCluster(cluster);
}

/*
 * Free a chain of clusters by setting their FAT entries to FAT_UNUSED.
 */

void
FreeClusterChain(cluster)
register word	cluster;
{
    register word   nextcluster;

    while (cluster != FAT_EOF) {
	nextcluster = NextCluster(cluster);
	SetFatEntry(cluster, FAT_UNUSED);
	Disk.freeclusts++;
	cluster = nextcluster;
    }
}

#endif				/* READONLY */

struct MSFileHandle *
MakeMSFileHandle(fl, mode)
struct MSFileLock *fl;
long		mode;
{
    struct MSFileHandle *fh;

    if (fl->msfl_Msd.msd_Attributes & ATTR_DIR) {
	error = ERROR_OBJECT_WRONG_TYPE;
	fh = NULL;
    } else if (fh = AllocMem((long) sizeof (*fh), MEMF_PUBLIC)) {
#ifndef READONLY
	/* Do we need to truncate the file? */
	if ((mode == MODE_NEWFILE) && fl->msfl_Msd.msd_Cluster) {
	    FreeClusterChain(fl->msfl_Msd.msd_Cluster);
	    fl->msfl_Msd.msd_Cluster = 0;
	    fl->msfl_Msd.msd_Filesize = 0;
	    UpdateFileLock(fl);
	}
#endif
	fh->msfh_Cluster = fl->msfl_Msd.msd_Cluster;
	fh->msfh_SeekPos = 0;
	fh->msfh_FileLock = fl;
#ifdef CONVERSIONS
	fh->msfh_Conversion = ConvNone;
#endif
    } else {
	error = ERROR_NO_FREE_STORE;
    }
    return fh;
}

/*
 * This routine opens a file.
 */

struct MSFileHandle *
MSOpen(parentdir, name, mode)
struct MSFileLock *parentdir;
char	       *name;
long		mode;
{
    struct MSFileLock *fl;
    struct MSFileHandle *fh = NULL;
    long	    lockmode;

    switch (mode) {
    case MODE_NEWFILE:
    case MODE_READWRITE:
	lockmode = EXCLUSIVE_LOCK ^ MODE_CREATEFILE;
	break;
    default:
	mode = MODE_OLDFILE;
    case MODE_OLDFILE:
	lockmode = SHARED_LOCK;
    }

#ifdef CONVERSIONS
    ConversionImbeddedInFileName = DefaultConversion;
#endif
    if (fl = MSLock(parentdir, name, lockmode)) {
makefh:
	fh = MakeMSFileHandle(fl, mode);
	if (fh) {
#ifdef CONVERSIONS
	    fh->msfh_Conversion = ConversionImbeddedInFileName;
#endif
	} else {
	    MSUnLock(fl);
	}

	return fh;
    }
#ifndef READONLY
    /*
     * If the file was not found, see if we can make a new one. Therefore
     * we need to have an empty spot in the desired directory, and create
     * an MSFileLock for it.
     */

    if (!(lockmode & MODE_CREATEFILE) && (fl = EmptyFileLock)) {
	debug(("Creating new file\n"));
	EmptyFileLock = NULL;
	fl->msfl_Msd.msd_Attributes = ATTR_ARCHIVED;
	UpdateFileLock(fl);
#ifndef CREATIONDATE_ONLY
	UpdateFileLock(fl->msfl_Parent);
#endif

	goto makefh;
    }
    if (EmptyFileLock) {
	MSUnLock(EmptyFileLock);
	EmptyFileLock = NULL;
    }
#endif

    return NULL;
}

long
MSClose(fh)
register struct MSFileHandle *fh;
{
    if (fh) {
	MSUnLock(fh->msfh_FileLock);
	FreeMem(fh, (long) sizeof (*fh));
    }
    return DOSTRUE;
}

long
FilePos(fh, position, mode)
struct MSFileHandle *fh;
long		position;
long		mode;
{
    switch (mode) {
    default:
    case OFFSET_BEGINNING:
	return position;
    case OFFSET_CURRENT:
	return fh->msfh_SeekPos + position;
	break;
    case OFFSET_END:
	return fh->msfh_FileLock->msfl_Msd.msd_Filesize + position;
    }
}

#ifdef ACTION_SET_FILE_SIZE
void
AdjustSeekPos(fh)
struct MSFileHandle *fh;
{
    if (fh->msfh_SeekPos > fh->msfh_FileLock->msfl_Msd.msd_Filesize) {
	/* fh->msfh_Cluster needs to be fully recalculated */
	MSSeek(fh, 0, OFFSET_BEGINNING);
	MSSeek(fh, 0, OFFSET_END);
    }
}
#endif

long
MSSeek(fh, position, mode)
struct MSFileHandle *fh;
long		position;
long		mode;
{
    long	    oldpos;
    long	    newpos;
    long	    filesize = fh->msfh_FileLock->msfl_Msd.msd_Filesize;
    word	    cluster = fh->msfh_Cluster;
    word	    oldcluster;
    word	    newcluster;

#ifdef ACTION_SET_FILE_SIZE
    if (fh->msfh_SeekPos > fh->msfh_FileLock->msfl_Msd.msd_Filesize) {
	fh->msfh_SeekPos = fh->msfh_FileLock->msfl_Msd.msd_Filesize;
    }
#endif
    oldpos = fh->msfh_SeekPos;
    newpos = FilePos(fh, position, mode);

    if (newpos < 0 || newpos > filesize) {
	error = ERROR_SEEK_ERROR;
	return -1;
    }
    newcluster = newpos / Disk.bpc;
    oldcluster = oldpos / Disk.bpc;

    if (oldcluster > newcluster) {      /* Seek backwards */
	cluster = fh->msfh_FileLock->msfl_Msd.msd_Cluster;
	oldcluster = 0;
    }
    if (oldcluster < newcluster) {
	if (CheckLock(fh->msfh_FileLock))
	    return -1;
	while (oldcluster < newcluster) {
	    cluster = NextCluster(cluster);
	    oldcluster++;
	}
    }
    fh->msfh_Cluster = cluster;
    fh->msfh_SeekPos = newpos;

    return oldpos;
}

long
MSRead(fh, userbuffer, size)
register struct MSFileHandle *fh;
register byte  *userbuffer;
register long	size;
{
    long	    oldsize;

    if (CheckLock(fh->msfh_FileLock))
	return -1;

#ifdef ACTION_SET_FILE_SIZE
    AdjustSeekPos(fh);
#endif
    if (fh->msfh_SeekPos + size > fh->msfh_FileLock->msfl_Msd.msd_Filesize)
	size = fh->msfh_FileLock->msfl_Msd.msd_Filesize - fh->msfh_SeekPos;

    oldsize = size;

    while (size > 0) {
	word		offset;
	word		sector;
	byte	       *diskbuffer;
	long		tocopy;

	offset = fh->msfh_SeekPos % Disk.bpc;
	sector = ClusterOffsetToSector(fh->msfh_Cluster, (word) offset);
	if (diskbuffer = ReadSec(sector)) {
	    offset %= Disk.bps;
	    tocopy = lmin(size, Disk.bps - offset);

#ifdef CONVERSIONS
	    (rd_Conv[fh->msfh_Conversion])(diskbuffer + offset, userbuffer,
					   tocopy);
#else
	    CopyMem(diskbuffer + offset, userbuffer, tocopy);
#endif
	    userbuffer += tocopy;
	    size -= tocopy;
	    FreeSec(diskbuffer);
	    /* MSSeek(fh, tocopy, (long) OFFSET_CURRENT); */
	    if ((fh->msfh_SeekPos += tocopy) % Disk.bpc == 0)
		fh->msfh_Cluster = NextCluster(fh->msfh_Cluster);
	} else {		/* Read error. Return amount successfully
				 * read, if any. Else return -1 for error. */
	    if (size == oldsize) {
		return -1;
	    }
	    return oldsize - size;
	}
    }

    return oldsize;
}

long
MSWrite(fh, userbuffer, size)
register struct MSFileHandle *fh;
register byte  *userbuffer;
register long	size;
{
#ifdef READONLY
    return -1;
#else
    long	    oldsize;
    struct MSFileLock *fl = fh->msfh_FileLock;
    word	    prevclust = fl->msfl_Msd.msd_Cluster;
    word	    update = 0;

    if (CheckLock(fl))
	return -1;

    if (fl->msfl_Msd.msd_Attributes & ATTR_READONLY) {
	error = ERROR_WRITE_PROTECTED;
	return -1;
    }

#ifdef ACTION_SET_FILE_SIZE
    AdjustSeekPos(fh);
#endif
    oldsize = size;
    /* Check if this will fit on the disk */
    {
	long		new;
	long		old;

	new = (fh->msfh_SeekPos + size + Disk.bpc - 1) / Disk.bpc;
	old = (fl->msfl_Msd.msd_Filesize + Disk.bpc - 1) / Disk.bpc;

	if (new - old > (long)Disk.freeclusts) {
	    error = ERROR_DISK_FULL;
	    goto some_error;
	}
    }

    while (size > 0) {
	/*
	 * Do we need to extend the file?
	 */

	if (fh->msfh_Cluster == 0 || fh->msfh_Cluster == FAT_EOF) {
	    word	    newclust;

	    newclust = ExtendClusterChain(prevclust);
	    debug(("Extend with %ld\n", (long)newclust));
	    if (newclust != FAT_EOF) {
		if (prevclust == 0) {   /* Record first cluster in dir */
		    fl->msfl_Msd.msd_Cluster = newclust;
		}
		fh->msfh_Cluster = newclust;
		prevclust = newclust;
	    } else {
		/* "Can't happen" */
		error = ERROR_DISK_FULL;
		goto some_error;
	    }
	}
	{
	    word	    offset;
	    word	    sector;
	    byte	   *diskbuffer;
	    long	    tocopy;

	    offset = fh->msfh_SeekPos % Disk.bpc;
	    sector = ClusterOffsetToSector(fh->msfh_Cluster, (word) offset);
	    offset %= Disk.bps;
	    tocopy = lmin(size, Disk.bps - offset);

	    if (offset == 0 && fh->msfh_SeekPos >= fl->msfl_Msd.msd_Filesize)
		diskbuffer = EmptySec(sector);
	    else
		diskbuffer = ReadSec(sector);

	    if (diskbuffer != NULL) {
#ifdef CONVERSIONS
		(wr_Conv[fh->msfh_Conversion])(userbuffer, diskbuffer + offset, tocopy);
#else
		CopyMem(userbuffer, diskbuffer + offset, tocopy);
#endif
		userbuffer += tocopy;
		size -= tocopy;
		MarkSecDirty(diskbuffer);
		FreeSec(diskbuffer);
		/* MSSeek(fh, tocopy, (long) OFFSET_CURRENT); */
		if ((fh->msfh_SeekPos += tocopy) % Disk.bpc == 0)
		    fh->msfh_Cluster = NextCluster(fh->msfh_Cluster);
		if (fh->msfh_SeekPos > fl->msfl_Msd.msd_Filesize)
		    fl->msfl_Msd.msd_Filesize = fh->msfh_SeekPos;
		fl->msfl_Msd.msd_Attributes |= ATTR_ARCHIVED;
		update = 1;
	    } else {		/* Write error. */
	some_error:
#ifndef CREATIONDATE_ONLY
		if (update)
		    UpdateFileLock(fl);
#endif
#if 1
		return -1;	/* We lose the information about how much
				 * data we wrote, but the standard file system
				 * seems to do it this way. */
#else
		if (size == oldsize) {
		    return -1;
		}
		return oldsize - size;	/* Amount successfully written */
#endif
	    }
	}
    }

#ifndef CREATIONDATE_ONLY
    if (update)
	UpdateFileLock(fl);
#endif

    return oldsize;
#endif
}

long
MSDeleteFile(parentdir, name)
struct MSFileLock *parentdir;
byte	       *name;
{
#ifdef READONLY
    return DOSFALSE;
#else
    register struct MSFileLock *fl;

    fl = MSLock(parentdir, name, EXCLUSIVE_LOCK);
    if (fl) {
	if (fl->msfl_Msd.msd_Attributes & ATTR_READONLY) {
	    error = ERROR_DELETE_PROTECTED;
	    goto some_error;
	}
	if (fl->msfl_Msd.msd_Attributes & ATTR_DIRECTORY) {
	    struct FileInfoBlock fib;

	    /*
	     * We normally can't get REAL exclusive locks on directories,
	     * so we check here just to be sure. We don't want to delete
	     * anyone's current directory, do we?
	     */

	    if (fl->msfl_Refcount != 1 || fl == RootLock) {
		error = ERROR_OBJECT_IN_USE;
		goto some_error;
	    }
	    if (MSExamine(fl, &fib) &&  /* directory itself */
		MSExNext(fl, &fib)) {   /* should fail */
		if (error == 0) {
	    not_empty:
		    error = ERROR_DIRECTORY_NOT_EMPTY;
	    some_error:
		    MSUnLock(fl);
		    return DOSFALSE;
		}
	    }
	    if (error != ERROR_NO_MORE_ENTRIES)
		goto some_error;

	    error = 0;
	}
	if (fl->msfl_Msd.msd_Cluster)
	    FreeClusterChain(fl->msfl_Msd.msd_Cluster);
	fl->msfl_Msd.msd_Name[0] = DIR_DELETED;
	WriteFileLock(fl);
#ifndef CREATIONDATE_ONLY
	UpdateFileLock(fl->msfl_Parent);
#endif
	MSUnLock(fl);

	return DOSTRUE;
    }
    return DOSFALSE;
#endif
}

long
MSSetDate(parentdir, name, datestamp)
struct MSFileLock *parentdir;
byte	       *name;
struct DateStamp *datestamp;
{
#ifdef READONLY
    return DOSFALSE;
#else
    register struct MSFileLock *fl;

    fl = MSLock(parentdir, name, EXCLUSIVE_LOCK);
    if (fl) {
	ToMSDate(&fl->msfl_Msd.msd_Date, &fl->msfl_Msd.msd_Time, datestamp);
	WriteFileLock(fl);
	MSUnLock(fl);

	return DOSTRUE;
    }
    return DOSFALSE;
#endif
}

/*
 * Create a new directory, with its own initial "." and ".." entries.
 */

struct MSFileLock *
MSCreateDir(parentdir, name)
struct MSFileLock *parentdir;
byte	       *name;
{
#ifdef READONLY
    return DOSFALSE;
#else
    register struct MSFileLock *fl;

    /*
     * Go create a new file. If we fail later, we have an empty file that
     * we delete again.
     */

    fl = MSLock(parentdir, name, EXCLUSIVE_LOCK ^ MODE_CREATEFILE);
    if (fl || error == ERROR_OBJECT_IN_USE) {
	error = ERROR_OBJECT_EXISTS;
	goto some_error;
    }
    if (error != 0) {
	goto some_error;
    }
    if (fl = EmptyFileLock) {
	debug(("Creating new dir\n"));
	EmptyFileLock = NULL;
	if ((fl->msfl_Msd.msd_Cluster = FindFreeCluster(FAT_EOF)) != FAT_EOF) {
	    struct MsDirEntry direntry;
	    byte	   *sec;
	    word	    sector;

	    sector = ClusterToSector(fl->msfl_Msd.msd_Cluster);
	    sec = EmptySec(sector);
	    if (sec == NULL)
		goto error_no_free_store;
	    setmem(sec, (int) Disk.bps, 0);

	    /*
	     * Turn the file into a directory.
	     */
	    fl->msfl_Msd.msd_Attributes = ATTR_DIRECTORY | ATTR_ARCHIVED;
	    fl->msfl_Refcount = 1;	/* Make it non-exclusive */
	    UpdateFileLock(fl);

	    /*
	     * Create the "." entry.
	     */
	    direntry = fl->msfl_Msd;
	    strncpy(direntry.msd_Name, DotDot + 1, L_8 + L_3);
	    OtherEndianMsd(&direntry);
	    ((struct MsDirEntry *) sec)[0] = direntry;

	    /*
	     * Get the real parent directory because we will duplicate the
	     * directory entry in the subdirectory.
	     */

	    parentdir = MSParentDir(fl);
	    if (parentdir == NULL)      /* Cannot happen */
		parentdir = MSDupLock(RootLock);
#ifndef CREATIONDATE_ONLY
	    UpdateFileLock(parentdir);
#endif

	    /*
	     * Create the ".." entry.
	     */
	    direntry = parentdir->msfl_Msd;
	    strncpy(direntry.msd_Name, DotDot, L_8 + L_3);
	    direntry.msd_Attributes = ATTR_DIRECTORY | ATTR_ARCHIVED;
	    OtherEndianMsd(&direntry);
	    ((struct MsDirEntry *) sec)[1] = direntry;

	    MSUnLock(parentdir);

	    MarkSecDirty(sec);
	    FreeSec(sec);

	    /*
	     * Clear out the rest of the newly created directory.
	     */

	    while ((sector = NextClusteredSector(sector)) != SEC_EOF) {
		sec = EmptySec(sector);
		if (sec == NULL)
		    goto error_no_free_store;
		setmem(sec, (int) Disk.bps, 0);
		MarkSecDirty(sec);
		FreeSec(sec);
	    }
	} else {
	    MSUnLock(fl);
	    fl = NULL;
	    MSDeleteFile(parentdir, name);
	    error = ERROR_DISK_FULL;
	}
    }
    if (EmptyFileLock) {
	MSUnLock(EmptyFileLock);
	EmptyFileLock = NULL;
    }
    return fl;

error_no_free_store:
    error = ERROR_NO_FREE_STORE;
some_error:
    if (fl)
	MSUnLock(fl);
    return DOSFALSE;
#endif
}

/*
 * Rename a file or directory, possibly moving it to a different
 * directory.
 *
 * "Tuned" to also work in full directories by first deleting the source
 * name, then look for a slot to put the destination name. If that fails,
 * we undo the deletion. By playing with the cache, we even avoid a write
 * of the sector with the undeleted entry.
 */

long
MSRename(slock, sname, dlock, dname)
struct MSFileLock *slock;
byte	       *sname;
struct MSFileLock *dlock;
byte	       *dname;
{
#ifdef READONLY
    return DOSFALSE;
#else
    struct MSFileLock *sfl;
    struct MSFileLock *dfl;
    long	    success;
    struct CacheSec *scache;
    ulong	    oldstatus;

    success = DOSFALSE;
    scache = NULL;
    dfl = NULL;

    sfl = MSLock(slock, sname, SHARED_LOCK);
    if (sfl == NULL || sfl == RootLock)
	goto some_error;

    /*
     * Now we are going to pull a dirty trick with the cache. We are going
     * to temporarily delete the source file, in the chache only, and
     * undelete it again if we cannot create the new name. And above all
     * we want to avoid unnecessary writes if we decide not to do the
     * deletion after all.
     */
    {
	byte	       *sec;
	byte		old;

	if ((sec = ReadSec(sfl->msfl_DirSector)) == NULL)
	    goto some_error;
	scache = FindSecByBuffer(sec);
	oldstatus = scache->sec_Refcount;

	old = sfl->msfl_Msd.msd_Name[0];
	sfl->msfl_Msd.msd_Name[0] = DIR_DELETED;
	WriteFileLock(sfl);
	sfl->msfl_Msd.msd_Name[0] = old;

	/*
	 * Don't FreeSec it yet; we don't want it written out to disk.
	 */
    }

    /*
     * Now we have freed the directory entry of the source name, we might
     * be able to use it for the destination name. But only if we also
     * temporarily hide the MSFileLock on that spot. Gross hack ahead!
     */

    sfl->msfl_DirOffset = ~sfl->msfl_DirOffset;
    dfl = MSLock(dlock, dname, EXCLUSIVE_LOCK ^ MODE_CREATEFILE);
    sfl->msfl_DirOffset = ~sfl->msfl_DirOffset;

    if (dfl != NULL || error == ERROR_OBJECT_IN_USE) {
	error = ERROR_OBJECT_EXISTS;
	goto undelete;
    }
    dfl = EmptyFileLock;
    EmptyFileLock = NULL;
    if (dfl == NULL) {
	/*
	 * Sigh, we could not create the new name. But because of that, we
	 * are sure that we need to write nothing to the disk at all. So
	 * we can safely reset the sector-dirty flag to what it was
	 * before, if we also restore the cached sector.
	 */
undelete:
	WriteFileLock(sfl);
	scache->sec_Refcount = oldstatus;
	goto some_error;
    }
    /*
     * Now, if the moved entry was a directory, and it was moved to a
     * different directory, we need to adapt its "..", which is the second
     * entry.
     */

    if (sfl->msfl_Msd.msd_Attributes & ATTR_DIRECTORY &&
	sfl->msfl_Parent != dfl->msfl_Parent) {
	struct MSFileLock *parentdir;
	struct MsDirEntry *dir;

	if (dir = (struct MsDirEntry *)
	    ReadSec(DirClusterToSector(sfl->msfl_Msd.msd_Cluster))) {
	    parentdir = MSParentDir(dfl);
	    /*
	     * Copy everything except the name which must remain "..". But
	     * first a quick consistency check...
	     */
	    debug(("Creating new \"..\"  "));
	    if (dir[1].msd_Name[1] == '.') {
		CopyMem(&parentdir->msfl_Msd.msd_Attributes,
			&dir[1].msd_Attributes,
			(long) sizeof (struct MsDirEntry) -
			OFFSETOF(MsDirEntry, msd_Attributes));
		dir[1].msd_Attributes = ATTR_DIRECTORY;
		OtherEndianMsd(&dir[1]);
		MarkSecDirty((byte *)dir);
	    }
#ifdef HDEBUG
	    else
		debug(("!!! No \"..\" found ??\n"));
#endif
	    MSUnLock(parentdir);
	    FreeSec((byte *)dir);
	}
    }
    /*
     * Move the name from the new entry to the old filelock. We do this
     * for the case that somebody else has a lock on the (possibly moved)
     * file/directory. Also move the other administration.
     */

    strncpy(sfl->msfl_Msd.msd_Name, dfl->msfl_Msd.msd_Name, L_8 + L_3);
    sfl->msfl_DirSector = dfl->msfl_DirSector;
    sfl->msfl_DirOffset = dfl->msfl_DirOffset;
    /*
     * Free the old, and get the new parent directory. They might be the
     * same, of course...
     */
    MSUnLock(sfl->msfl_Parent);
    sfl->msfl_Parent = dfl->msfl_Parent;
    dfl->msfl_Parent = NULL;
    sfl->msfl_Msd.msd_Attributes |= ATTR_ARCHIVED;
    WriteFileLock(sfl);         /* Write the new name; the old name
				 * already has been deleted. */
#ifndef CREATIONDATE_ONLY
    UpdateFileLock(sfl->msfl_Parent);
    UpdateFileLock(dfl->msfl_Parent);
#endif
    success = DOSTRUE;

some_error:
    if (sfl)
	MSUnLock(sfl);
    if (dfl)
	MSUnLock(dfl);
    if (scache)
	FreeSec(scache->sec_Data);

    return success;
#endif
}
