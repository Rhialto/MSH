/*-
 * $Id: hanfile.c,v 1.4 90/01/23 02:32:23 Rhialto Exp Locker: Rhialto $
 * $Log:	hanfile.c,v $
 * Revision 1.4  90/01/23  02:32:23  Rhialto
 * Add 16-bit FAT support.
 *
 * Revision 1.3  90/01/23  00:39:04  Rhialto
 * Always return -1 on MSWrite error.
 *
 * Revision 1.2  89/12/17  23:04:39  Rhialto
 * Add ATTR_READONLY support
 *
 * Revision 1.1  89/12/17  20:03:11  Rhialto
 * Initial revision
 *
 * HANFILE.C
 *
 * The code for the messydos file system handler.
 *
 * This parts handles files and the File Allocation Table.
 *
 * This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
-*/

#include "han.h"
#include "dos.h"

#ifdef DEBUG
#   define	debug(x)  dbprintf x
#else
#   define	debug(x)
#endif

extern char	DotDot[1 + 8 + 3];

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
	if (secptr = GetSec(Disk.res + i)) {
	    CopyMem(secptr, Fat + i * Disk.bps, (long) Disk.bps);
	    FreeSec(secptr);
	} else {
	    /* q&d way to set the entire FAT to FAT_EOF */
	    setmem(Fat + i * Disk.bps, (int) Disk.bps, FAT_EOF);        /* 0xFF */
	}
    }

    debug(("counting free clusters\n"));

    Disk.nsectsfree = 0;
    for (i = MS_FIRSTCLUST; i <= Disk.maxclst; i++) {
	if (GetFatEntry((word) i) == FAT_UNUSED)
	    Disk.nsectsfree += Disk.spc;
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
	 * Convert the special values 0xFF0 .. 0xFFF to 16 bits so they
	 * can be checked consistently.
	 */
	if (twoentries >= 0xFF0)
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

    debug(("SetFatEntry %d to %d\n", cluster, value));

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

    if (Disk.nsectsfree >= Disk.spc) {
	for (i = prev + 1; i != prev; i++) {
	    if (i > Disk.maxclst)       /* Wrap around */
		i = MS_FIRSTCLUST;
	    if (GetFatEntry(i) == FAT_UNUSED) {
		SetFatEntry(i, FAT_EOF);
		if (prev >= MS_FIRSTCLUST)
		    SetFatEntry(prev, i);
		Disk.nsectsfree -= Disk.spc;
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
    if (cluster != 0)
	while ((nextcluster = NextCluster(cluster)) != FAT_EOF) {
	    cluster = nextcluster;
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
	debug(("Freeing cluster %d\n", cluster));
	nextcluster = NextCluster(cluster);
	SetFatEntry(cluster, FAT_UNUSED);
	Disk.nsectsfree += Disk.spc;
	cluster = nextcluster;
    }
}

#endif				/* READONLY */

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

    if (fl = MSLock(parentdir, name, lockmode)) {
makefh:
	if (fl->msfl_Msd.msd_Attributes & ATTR_DIR) {
	    error = ERROR_OBJECT_WRONG_TYPE;
	    MSUnLock(fl);
	} else if (fh = AllocMem((long) sizeof (*fh), MEMF_PUBLIC)) {
#ifndef READONLY
	    /* Do we need to truncate the file? */
	    if (mode == MODE_NEWFILE && fl->msfl_Msd.msd_Cluster) {
		FreeClusterChain(fl->msfl_Msd.msd_Cluster);
		fl->msfl_Msd.msd_Cluster = 0;
		fl->msfl_Msd.msd_Filesize = 0;
		UpdateFileLock(fl);
	    }
#endif
	    fh->msfh_Cluster = fl->msfl_Msd.msd_Cluster;
	    fh->msfh_SeekPos = 0;
	    fh->msfh_FileLock = fl;
	} else {
	    error = ERROR_NO_FREE_STORE;
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

	goto makefh;
    }
    if (EmptyFileLock) {
	MSUnLock(EmptyFileLock);
	EmptyFileLock = NULL;
    }
#endif

    return NULL;
}

void
MSClose(fh)
register struct MSFileHandle *fh;
{
    if (fh) {
	MSUnLock(fh->msfh_FileLock);
	FreeMem(fh, (long) sizeof (*fh));
    }
}

long
MSSeek(fh, position, mode)
struct MSFileHandle *fh;
long		position;
long		mode;
{
    long	    oldpos = fh->msfh_SeekPos;
    long	    newpos = oldpos;
    long	    filesize = fh->msfh_FileLock->msfl_Msd.msd_Filesize;
    word	    cluster = fh->msfh_Cluster;
    word	    oldcluster;
    word	    newcluster;

    switch (mode) {
    case OFFSET_BEGINNING:
	newpos = position;
	break;
    case OFFSET_CURRENT:
	newpos += position;
	break;
    case OFFSET_END:
	newpos = filesize - position;
	break;
    }

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
	    return -1L;
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
	return -1L;

    if (fh->msfh_SeekPos + size > fh->msfh_FileLock->msfl_Msd.msd_Filesize)
	size = fh->msfh_FileLock->msfl_Msd.msd_Filesize - fh->msfh_SeekPos;

    oldsize = size;

    while (size > 0) {
	word		offset;
	word		sector;
	byte	       *diskbuffer;
	long		insector;
	long		tocopy;

	offset = fh->msfh_SeekPos % Disk.bpc;
	sector = ClusterOffsetToSector(fh->msfh_Cluster, (word) offset);
	if (diskbuffer = GetSec(sector)) {
	    offset %= Disk.bps;
	    insector = Disk.bps - offset;
	    tocopy = lmin(size, insector);

	    CopyMem(diskbuffer + offset, userbuffer, tocopy);
	    userbuffer += tocopy;
	    size -= tocopy;
	    FreeSec(diskbuffer);
	    /* MSSeek(fh, tocopy, (long) OFFSET_CURRENT); */
	    if ((fh->msfh_SeekPos += tocopy) % Disk.bpc == 0)
		fh->msfh_Cluster = NextCluster(fh->msfh_Cluster);
	} else {		/* Read error. Return amount successfully
				 * read, if any. Else return -1 for error. */
	    if (size == oldsize) {
		return -1L;
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

    if (CheckLock(fl))
	return -1;

    if (fl->msfl_Msd.msd_Attributes & ATTR_READONLY) {
	error = ERROR_WRITE_PROTECTED;
	return -1;
    }

    oldsize = size;

    while (size > 0) {
	/*
	 * Do we need to extend the file?
	 */

	if (fh->msfh_Cluster == 0 || fh->msfh_Cluster == FAT_EOF) {
	    word	    newclust;

	    newclust = ExtendClusterChain(prevclust);
	    debug(("Extended file with %d\n", newclust));
	    if (newclust != FAT_EOF) {
		if (prevclust == 0) {   /* Record first cluster in dir */
		    fl->msfl_Msd.msd_Cluster = newclust;
		}
		fh->msfh_Cluster = newclust;
		prevclust = newclust;
	    } else {
		error = ERROR_DISK_FULL;
		goto error;
	    }
	} {
	    word	    offset;
	    word	    sector;
	    byte	   *diskbuffer;
	    long	    insector;
	    long	    tocopy;

	    offset = fh->msfh_SeekPos % Disk.bpc;
	    sector = ClusterOffsetToSector(fh->msfh_Cluster, (word) offset);
	    offset %= Disk.bps;
	    insector = Disk.bps - offset;
	    tocopy = lmin(size, insector);

	    if (tocopy == Disk.bps)
		diskbuffer = EmptySec(sector);
	    else
		diskbuffer = GetSec(sector);

	    if (diskbuffer != NULL) {
		CopyMem(userbuffer, diskbuffer + offset, tocopy);
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
		UpdateFileLock(fl);
	    } else {		/* Write error. */
	error:
#if 1
		return -1;	/* We loose the information about how much
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
	    goto error;
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
		goto error;
	    }
	    if (MSExamine(fl, &fib) &&  /* directory itself */
		MSExNext(fl, &fib)) {   /* should fail */
		if (error == 0) {
	    not_empty:
		    error = ERROR_DIRECTORY_NOT_EMPTY;
	    error:
		    MSUnLock(fl);
		    return DOSFALSE;
		}
	    }
	    if (error != ERROR_NO_MORE_ENTRIES)
		goto error;

	    error = 0;
	}
	if (fl->msfl_Msd.msd_Cluster)
	    FreeClusterChain(fl->msfl_Msd.msd_Cluster);
	fl->msfl_Msd.msd_Name[0] = DIR_DELETED;
	WriteFileLock(fl);
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
	goto error;
    }
    if (error != 0) {
	goto error;
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
	    fl->msfl_Msd.msd_Attributes = ATTR_DIRECTORY;
	    UpdateFileLock(fl);

	    /*
	     * Create the "." entry.
	     */
	    direntry = fl->msfl_Msd;
	    strncpy(direntry.msd_Name, DotDot + 1, 8 + 3);
	    OtherEndianMsd(&direntry);
	    ((struct MsDirEntry *) sec)[0] = direntry;

	    /*
	     * Get the real parent directory because we will duplicate the
	     * directory entry in the subdirectory.
	     */

	    parentdir = MSParentDir(fl);
	    if (parentdir == NULL)      /* Cannot happen */
		parentdir = MSDupLock(RootLock);

	    /*
	     * Create the ".." entry.
	     */
	    direntry = parentdir->msfl_Msd;
	    strncpy(direntry.msd_Name, DotDot, 8 + 3);
	    direntry.msd_Attributes = ATTR_DIRECTORY;
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
error:
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
	goto error;

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

	if ((sec = GetSec(sfl->msfl_DirSector)) == NULL)
	    goto error;
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
	goto error;
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
	    GetSec(DirClusterToSector(sfl->msfl_Msd.msd_Cluster))) {
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
		MarkSecDirty(dir);
	    }
#ifdef DEBUG
	    else
		debug(("!!! No \"..\" found ??\n"));
#endif
	    MSUnLock(parentdir);
	    FreeSec(dir);
	}
    }
    /*
     * Move the name from the new entry to the old filelock. We do this
     * for the case that somebody else has a lock on the (possibly moved)
     * file/directory. Also move the other administration.
     */

    strncpy(sfl->msfl_Msd.msd_Name, dfl->msfl_Msd.msd_Name, 8 + 3);
    sfl->msfl_DirSector = dfl->msfl_DirSector;
    sfl->msfl_DirOffset = dfl->msfl_DirOffset;
    /*
     * Free the old, and get the new parent directory. They might be the
     * same, of course...
     */
    MSUnLock(sfl->msfl_Parent);
    sfl->msfl_Parent = dfl->msfl_Parent;
    dfl->msfl_Parent = NULL;
    sfl->msfl_Msd.msd_Attributes &= ~ATTR_ARCHIVED;
    WriteFileLock(sfl);         /* Write the new name; the old name
				 * already has been deleted. */
    success = DOSTRUE;

error:
    if (sfl)
	MSUnLock(sfl);
    if (dfl)
	MSUnLock(dfl);
    if (scache)
	FreeSec(scache->sec_Data);

    return success;
#endif
}
