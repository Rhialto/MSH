/*-
 * $Id: hanmain.c,v 1.40 91/03/03 17:46:39 Rhialto Rel $
 * $Log:	hanmain.c,v $
 * Revision 1.40  91/03/03  17:46:39  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.32  90/11/23  23:54:07  Rhialto
 * Prepare for syslog
 *
 * Revision 1.31  90/11/10  02:46:35  Rhialto
 * Patch 3a. Changes location of disk volume date.
 *
 * Revision 1.30  90/06/04  23:16:50  Rhialto
 * Release 1 Patch 3
 *
 *  HANMAIN.C
 *
 *  The code for the messydos file system handler.
 *
 *  Some start/stop stuff that is not really part of the
 *  file system itself but that must be done anyway.
 *
 *  This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 *  not be used or copied without a licence.
-*/

#include <amiga.h>
#include <functions.h>
#include <string.h>
#include "han.h"
#include "dos.h"

#ifdef HDEBUG
#   define	debug(x)  syslog x
    void initsyslog(void);
    void syslog(char *, ...);
    void uninitsyslog(void);
#else
#   define	debug(x)
#endif

extern int	CheckBootBlock;
extern char	DotDot[1 + 8 + 3];
struct Library *IntuitionBase;
static char RCSId[] = "Messydos filing system $Revision: 1.40 $ $Date: 91/03/03 17:46:39 $, by Olaf Seibert";

byte
ToUpper(ch)
register byte	ch;
{
    if (ch >= 'a' && ch <= 'z')
	return ch + ('A' - 'a');
    if (ch == '.')
	return '!';
    return ch & ~DIR_DELETED_MASK;
}

long
lmin(a, b)
long		a,
		b;
{
    return (a < b) ? a : b;
}

byte	       *
ZapSpaces(begin, end)
register byte  *begin,
	       *end;
{
    while (end > begin && end[-1] == ' ')
	*--end = '\0';

    return end;
}

/*
 * Map an arbitrary file name to MS-DOS conventions. The output format is
 * 8+3 without dot, padded with spaces, suitable for direct comparison
 * with directory entries. Return a pointer to the delimiter found ('\0'
 * or '/'). [[Make sure that Examine/ExNext return a proper inverse of
 * this...]]
 */

byte	       *
ToMSName(dest, source)
byte	       *dest;
register byte  *source;
{
    byte	   *dotp;
    byte	   *slashp;
    register int    i,
		    len;

    if (*source == '/') {       /* parentdir */
	strncpy(dest, DotDot, 8 + 3);   /* ".." */
	return source;
    }
    /*
     * Remove any strictly leading dots. .info -> info, .indent.pro ->
     * indent.pro, .profile -> profile, etc.
     */
    while (*source == '.')
	source++;

    /*
     * Find dot and slash which are delimiters of name and extension.
     */
    {
	register byte  *cp;

	cp = source;
	while (*cp) {
	    if (*cp == '.' || *cp == '/')
		break;
	    cp++;
	}
	dotp = cp;
	while (*cp) {
	    if (*cp == '/')
		break;
	    cp++;
	}
	slashp = cp;
    }

    len = dotp - source;
    if (len > 8)
	len = 8;

    for (i = 0; i < len; i++) {
	*dest++ = ToUpper(*source++);
    }
    for (; i < 8; i++) {
	*dest++ = ' ';
    }

    source = dotp + 1;
    len = slashp - source;	/* so will be -1 if no suffix */
    if (len > 3)
	len = 3;

    for (i = 0; i < len; i++) {
	*dest++ = ToUpper(*source++);
    }
    for (; i < 3; i++) {
	*dest++ = ' ';
    }

    return slashp;
}

/*
 * Do the Info call.
 */

long
MSDiskInfo(infodata)
struct InfoData *infodata;
{
    extern DEVLIST *VolNode;

    setmem(infodata, sizeof (*infodata), 0);

    infodata->id_DiskState = IDDiskState;
    infodata->id_DiskType = IDDiskType;
    infodata->id_UnitNumber = UnitNr;

    infodata->id_VolumeNode = (BPTR) CTOB(VolNode);
    infodata->id_InUse = LockList ? 1 : 0;

    if (IDDiskType == ID_DOS_DISK) {
	infodata->id_NumBlocks = Disk.nsects;
	infodata->id_NumBlocksUsed = Disk.nsects - Disk.nsectsfree;
	infodata->id_BytesPerBlock = Disk.bps;
    }
    return DOSTRUE;
}

/*
 * We (re-)establish our List of MSFileLocks after a disk has been
 * (re-)inserted. If there are no known locks, we make the root lock from
 * the volume label, if there is one.
 *
 * We get a special cookie to hand to a cleanup routine that we must call
 * when finally all locks on the current disk are UnLock()ed. (this is
 * actually the volume node, but we don't want to know that.)
 *
 * This must be called some time after IdentifyDisk().
 */

void
MSDiskInserted(locks, cookie)
register struct LockList **locks;
void	       *cookie;
{
    debug(("MSDiskInserted %08lx\n", cookie));

    LockList = *locks;

    if (LockList == NULL) {
	LockList = NewLockList(cookie);
	RootLock = MakeLock(NULL, &Disk.vollabel, SHARED_LOCK);
    } else {
	RootLock = MSDupLock(GetTail(&LockList->ll_List));
    }

    InitCacheList();
}

/*
 * Remove the current disk. A place is offered to save the current
 * LockList to restore later. We must unlock the root lock since it isn't
 * a real reference to the disk, just a placeholder for dummies that hand
 * us NULL locks.
 */

int
MSDiskRemoved(locks)
register struct LockList **locks;
{
#ifndef READONLY
    if (FatDirty || (DelayState & DELAY_DIRTY))
	MSUpdate(1);            /* Force a requester */
#endif

    FreeFat();
    FreeCacheList();

    IDDiskType = ID_NO_DISK_PRESENT;
    *locks = NULL;

    if (RootLock == NULL) {
	debug(("MSDiskRemoved with no RootLock\n"));
	return 1;
    }
#ifdef HDEBUG
    if (RootLock != GetTail(&LockList->ll_List)) {
	debug(("RootLock not at end of LockList!\n"));
	/* Get the lock on the root dir at the tail of the List */
	Remove((struct Node *)RootLock);
	AddTail((struct List *)&LockList->ll_List, (struct Node *)RootLock);
    }
#endif

    /*
     * If there are no real locks on the disk, we need not keep any
     * information about it.
     */

    MSUnLock(RootLock);         /* may call FreeLockList and free VolNode
				 * (!) */
    RootLock = NULL;

    if (LockList) {
	*locks = LockList;	/* VolNode can't be gone now... */
	LockList = NULL;
	return 0;		/* not all references gone */
    } else {
	return 1;		/* all gone, even the VolNode */
    }
}

void
HanCloseDown()
{
#ifdef HDEBUG
    register struct MSFileLock *fl;

    while (LockList && (fl = (struct MSFileLock *) GetHead(&LockList->ll_List))) {
	debug(("UNLOCKING %08lx: ", fl));
	PrintDirEntry((struct DirEntry *)&fl->msfl_Msd);
	MSUnLock(fl);           /* Remove()s it from this List */
    }
#endif
    if (DiskIOReq) {
	if (DiskIOReq->iotd_Req.io_Unit) {
	    MSUpdate(1);
	    CloseDevice((struct IORequest *)DiskIOReq);
	}
	DeleteExtIO((struct IORequest *)DiskIOReq);
	DiskIOReq = NULL;
    }
    if (TimeIOReq) {
	if (TimeIOReq->tr_node.io_Unit) {
	    WaitIO((struct IORequest *)TimeIOReq);
	    CloseDevice((struct IORequest *)TimeIOReq);
	}
	DeleteExtIO((struct IORequest *)TimeIOReq);
	TimeIOReq = NULL;
    }
    if (DiskReplyPort) {
	DeletePort(DiskReplyPort);
	DiskReplyPort = NULL;
    }
    if (IntuitionBase) {
	CloseLibrary(IntuitionBase);
	IntuitionBase = NULL;
    }
}

int
HanOpenUp()
{
    LockList = NULL;
    RootLock = NULL;
    Fat = NULL;
    IDDiskState = ID_WRITE_PROTECTED;
    IDDiskType = ID_NO_DISK_PRESENT;
    DelayState = DELAY_OFF;
    Disk.bps = MS_BPS;
    CheckBootBlock = CHECK_BOOTJMP | CHECK_SANITY | CHECK_SAN_DEFAULT;
    InitCacheList();

    TimeIOReq = NULL;

#ifdef HDEBUG
    if (!(DiskReplyPort = CreatePort("MSH:disk.replyport", -1L)))
	goto abort;
#else
    if (!(DiskReplyPort = CreatePort(NULL, -1L)))
	goto abort;
#endif

    debug(("DiskReplyPort = 0x%08lx\n", DiskReplyPort));

    if (!(DiskIOReq = CreateExtIO(DiskReplyPort, (long) sizeof (*DiskIOReq)))) {
	debug(("Failed to CreateExtIO\n"));
	goto abort;
    }
    if (OpenDevice(DevName, UnitNr, (struct IORequest *)DiskIOReq,
		   DevFlags | TDF_ALLOW_NON_3_5)) {
	debug(("Failed to OpenDevice\n"));
	goto abort;
    }
    TimeIOReq = (struct timerequest *) CreateExtIO(DiskReplyPort,
					     (long) sizeof (*TimeIOReq));

    if (TimeIOReq == NULL || OpenDevice(TIMERNAME, UNIT_VBLANK,
					(struct IORequest *)TimeIOReq, 0L))
	goto abort;
    TimeIOReq->tr_node.io_Flags = IOF_QUICK;	/* For the first WaitIO() */

    IntuitionBase = OpenLibrary("intuition.library", 0L);
    return DOSTRUE;

abort:
    HanCloseDown();
    return 0;
}

/*
 * Relabel the disk. We create new labels if necessary.
 */

long
MSRelabel(newname)
byte	       *newname;
{
#ifdef READONLY
    return DOSFALSE;
#else
    /*
     * A null or empty string means: remove the label, if any.
     */
    if (!newname || !*newname) {
	if (RootLock->msfl_DirSector != (word)ROOT_SEC) {
	    RootLock->msfl_Msd.msd_Name[0] = DIR_DELETED;
	    RootLock->msfl_Msd.msd_Attributes = 0;
	    WriteFileLock(RootLock);
	    Disk.vollabel = FakeRootDirEntry;
	    RootLock->msfl_Msd = Disk.vollabel.de_Msd;
	    RootLock->msfl_DirSector = Disk.vollabel.de_Sector;
	    RootLock->msfl_DirOffset = Disk.vollabel.de_Offset;
	}
	return DOSTRUE;
    }

    /*
     * No label yet? Then we must create one, even if we need to move
     * something else for it.
     */

    if (RootLock->msfl_DirSector == (word)ROOT_SEC) {
	struct MSFileLock *new;

	new = MSLock(RootLock, "OLAF-><>.\\*", EXCLUSIVE_LOCK ^ MODE_CREATEFILE);
	if ((new == NULL) && (new = EmptyFileLock)) {
	    struct DateStamp dateStamp;

	    error = 0;

	    DateStamp(&dateStamp);
	    ToMSDate(&Disk.vollabel.de_Msd.msd_CreationDate,
		     &Disk.vollabel.de_Msd.msd_CreationTime, &dateStamp);

	    Disk.vollabel.de_Msd.msd_Date =
		Disk.vollabel.de_Msd.msd_CreationDate;
	    Disk.vollabel.de_Msd.msd_Time =
		Disk.vollabel.de_Msd.msd_CreationTime;

	    if (new->msfl_DirSector == Disk.rootdir) {
		RootLock->msfl_DirSector = Disk.rootdir;
		RootLock->msfl_DirOffset = new->msfl_DirOffset;
	    } else {
		/*
		 * Move something out of the first directory block. Try
		 * not to move system files or directories (. ..), but
		 * we'll do it if we need to. Set the root dir date to
		 * now.
		 */
		byte	       *fromsec;
		byte	       *tosec;
		register struct MsDirEntry *dir;

		fromsec = GetSec(Disk.rootdir);
		tosec = GetSec(new->msfl_DirSector);

		dir = (struct MsDirEntry *) fromsec;
		while (dir->msd_Attributes & (ATTR_SYSTEM | ATTR_DIRECTORY)) {
		    if ((byte *) ++dir >= fromsec + Disk.bps) {
			--dir;	/* Back to last entry in the block */
			break;	/* and move it no matter what */
		    }
		}
		CopyMem((byte *)dir, tosec + new->msfl_DirOffset,
			(long) sizeof (struct MsDirEntry));
		MarkSecDirty(tosec);
		RootLock->msfl_DirSector = Disk.rootdir;
		RootLock->msfl_DirOffset = (byte *) dir - fromsec;

		FreeSec(tosec);
		FreeSec(fromsec);

	    }
	}
	EmptyFileLock = NULL;
	MSUnLock(new);
    }
    if (error == 0) {
	/*
	 * The easy part: Copy the name to Disk.vollabel and RootLock.
	 */
	{
	    register int    i;
	    register byte  *s,
			   *d;

	    s = newname;
	    d = Disk.vollabel.de_Msd.msd_Name;
	    for (i = 0; i < 8 + 3; i++) {
		if (s[0])
		    *d++ = ToUpper(*s++);
		else
		    *d++ = ' ';
	    }
	}
	RootLock->msfl_Msd = Disk.vollabel.de_Msd;	/* Just for the name and
							 * dates */
	WriteFileLock(RootLock);

	return DOSTRUE;
    }
    return DOSFALSE;
#endif
}

#ifdef HDEBUG

_abort()
{
    HanCloseDown();
    RemTask(NULL);
}

#endif				/* HDEBUG */
