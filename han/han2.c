/*-
 * $Id: han2.c,v 1.52 92/09/06 00:20:34 Rhialto Exp $
 * $Log:	han2.c,v $
 * Revision 1.52  92/09/06  00:20:34  Rhialto
 *
 * Revision 1.46  91/10/06  18:26:04  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.43  91/09/28  01:30:07  Rhialto
 * Preliminary - not functional yet
 *
 *
 * HAN2.C
 *
 * The code for the messydos file system handler.
 *
 * New functions to make 2.0 stuff work.
 *
 * This code is (C) Copyright 1991-1992 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include <functions.h>
#include "han.h"
#include "dos.h"

#ifdef ACTION_SAME_LOCK      /* Do we have 2.0 includes? */

#ifdef HDEBUG
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

/*	DOS Object Management */
BPTR DupLockFromFH( BPTR fh );
BPTR OpenFromLock( BPTR lock );
BPTR ParentOfFH( BPTR fh );
BOOL ExamineFH( BPTR fh, struct FileInfoBlock *fib );
LONG SetFileDate( UBYTE *name, struct DateStamp *date );
LONG NameFromLock( BPTR lock, UBYTE *buffer, long len );
LONG NameFromFH( BPTR fh, UBYTE *buffer, long len );
LONG SameLock( BPTR lock1, BPTR lock2 );
LONG SetMode( BPTR fh, long mode );
LONG ExAll( BPTR lock, struct ExAllData *buffer, long size, long data,
	struct ExAllControl *control );
LONG ReadLink( struct MsgPort *port, BPTR lock, UBYTE *path, UBYTE *buffer,
	unsigned long size );
LONG MakeLink( UBYTE *name, long dest, long soft );
LONG ChangeMode( long type, BPTR fh, long newmode );
LONG SetFileSize( BPTR fh, long pos, long mode );
/*	Device List Management */
BOOL IsFileSystem( UBYTE *name );
/*	Handler Interface */
BOOL Format( UBYTE *filesystem, UBYTE *volumename, unsigned long dostype );
/*	Notification */
BOOL StartNotify( struct NotifyRequest *notify );
void EndNotify( struct NotifyRequest *notify );

/*
 * We are taking great care to share lock structures between locks on
 * the same file. It is here, among other places, that we exploit that.
 */
long
MSSameLock(lock1, lock2)
struct MSFileLock *lock1, *lock2;
{
    if (lock1 == lock2)
	return LOCK_SAME;
    else
	return LOCK_SAME_HANDLER;
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
    UpdateFileLock(msfl);
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
	while (((char *)(ead + 1) + 8+1+3 + 1) <= end &&
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

#endif /* ACTION_COMPARE_LOCK */
