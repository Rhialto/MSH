/*-
 * $Id$
 * $Log$
 *
 * HAN2.C
 *
 * The code for the messydos file system handler.
 *
 * New functions to make 2.0 stuff work.
 *
 * This code is (C) Copyright 1991 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
-*/

#include <amiga.h>
#include <functions.h>
#include "han.h"
#include "dos.h"
#include <dos/exall.h>

#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

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
    return lock1 == lock2;
}

struct MSFileLock *
MSDupLockFromFH(msfh)
struct MSFileHandle *msfh;
{
    return MSDupLock(msfh->msfh_FileLock);
}

struct MSFileLock *
MSParentOfFH(msfh)
struct MSFileHandle *msfh;
{
    return MSParentDir(msfh->msfh_FileLock);
}

long
MSExamineFH(msfh, fib)
struct MSFileHandle *msfh;
struct FileInfoBlock *fib;
{
    return MSExamine(msfh->msfh_FileLock);
}

long
MSSetFileSize(msfh, pos, mode)
struct MSFileHandle *msfh;
long		pos;
long		mode;	/* ??? */
{
    long	    clusters;
    struct MSFileLock *msfl;
    long success = FALSE;

    msfl = msfh->msfh_FileLock;

    if (msfl->msfl_Msd.msd_Attributes & ATTR_READONLY) {
	error = ERROR_WRITE_PROTECTED;
	return ;
    }

    oldclusters = (msfl->msfl_Msd.msd_Filesize + Disk.bpc - 1) / Disk.bpc;
    newclusters = (pos + Disk.bpc - 1) / Disk.bpc;

    if (newclusters == oldclusters) {
	/* do nothing */
	success = TRUE;
    } else if (newclusters > oldclusters) {
	/* extend the file */
	if ((newclusters - oldclusters) <= (Disk.nsectsfree / Disk.spc)) {
	    while (oldclusters < newclusters) {
		cluster = ExtendClusterChain(cluster);
		oldclusters++;
	    }
	    success = TRUE;
	} else
	    error = ERROR_DISK_FULL;
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
	    FreeClusterChain(cluster);
	    SetFatEntry(cluster, FAT_EOF);
	}
	success = TRUE;
    }

    if (success) {
	msfl->msfl_Msd.msd_Filesize = pos;
	UpdateFileLock(msfl);
    }
    return msfl->msfl_Msd.msd_Filesize;
}

long
MSChangeMode(type, fh, newmode)
{
}

/*
 * ExAll can conveniently be done with Examine/ExNext... at least for now.
 */

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

long
MSLockRecords()
{
}

long
MSUnLockRecords()
{
}

MSStartNotify(req)
struct NotifyRequest *req;
{
}

MSEndNotify(req)
struct NotifyRequest *req;
{
}
