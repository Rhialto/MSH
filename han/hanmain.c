/*-
 * $Id: hanmain.c,v 1.58 2005/10/19 16:53:52 Rhialto Exp $
 * $Log: hanmain.c,v $
 * Revision 1.58  2005/10/19  16:53:52  Rhialto
 * Finally a new version!
 *
 * Revision 1.56  1996/12/22  00:22:33  Rhialto
 * Add IEQUALIFIER_MULTIBROADCAST to disk inserted/removed events.
 *
 * Revision 1.55  1993/12/30  23:02:45	Rhialto
 * New LONGNAMES filesystem, changes throughout the handler.
 * Optional (compile-time) broadcast IECLASS_DISKINSERTED messages.
 * Don't fail Info() if there is no disk in drive.
 * Freeze for MAXON5.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:25:46  Rhialto
 * Expose private info if user asks nicely.
 *
 * Revision 1.52  92/09/06  00:19:31  Rhialto
 * Include $VER in version string.
 *
 * Revision 1.51  92/04/17  15:36:35  Rhialto
 * Freeze for MAXON. removed InitCacheList() from MSDiskInserted().
 *
 * Revision 1.46  91/10/06  18:27:36  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.45  91/10/03  23:36:47  Rhialto
 * Implement conversions during Read()/Write()
 *
 * Revision 1.43  91/09/28  01:40:24  Rhialto
 * Changed to newer syslog stuff.
 *
 * Revision 1.42  91/06/13  23:50:21  Rhialto
 * DICE conversion
 *
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
 *  This code is (C) Copyright 1989-1996 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#include "han.h"
#include "dos.h"
#if CONVERSIONS
#   include "hanconv.h"
#endif
#if INPUTDEV
#include <devices/input.h>
#include <devices/inputevent.h>
#include <clib/intuition_protos.h>
#endif

#include <string.h>
#if HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

Prototype byte ToUpper(byte ch);
Prototype long lmin(long a, long b);
Prototype byte *ZapSpaces(byte *begin, byte *end);
Prototype byte *ToMSName(byte *dest, byte *source);
Prototype long MSDiskInfo(struct InfoData *infodata);
Prototype void MSDiskInserted(struct LockList **locks, void *cookie);
Prototype int MSDiskRemoved(struct LockList **locks);
Prototype void InputDiskInserted(void);
Prototype void InputDiskRemoved(void);
Prototype void HanCloseDown(void);
Prototype int HanOpenUp(void);
Prototype long MSRelabel(byte *newname);
Prototype struct PrivateInfo *PrivateInfo(void);

struct Library *IntuitionBase;
#if INPUTDEV
struct IOStdReq *InputIOReq;
#endif
Local const char RCSId[] = "\0$""VER: Messydos filing system $Revision: 1.58 $ $Date: 2005/10/19 16:53:52 $, by Olaf Seibert";

#define CONV_SEP    ';'

byte
ToUpper(ch)
byte		ch;
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
byte	       *begin,
	       *end;
{
    *end = '\0';                /* Make sure the string is 0-terminated */

    while (end > begin && end[-1] == ' ')
	*--end = '\0';

    return end;
}

#if VFATSUPPORT
Prototype byte		*ComponentEnd(char *source);

byte		*
ComponentEnd(char *source)
{
    byte *end = source;

    while (*end != '\0' && *end != '/')
	end++;
    /* XXX should do something about ;X before the end */
    return end;
}

#else

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
byte	       *source;
{
#if LONGNAMES == 0
    byte	   *dotp;
    byte	   *slashp;
    int 	    i;
#endif
    int 	    len;

    if (*source == '/') {       /* parentdir */
	strncpy(dest, DotDot, L_8 + L_3);   /* ".." */
	return source;
    }
#if LONGNAMES
    /* Pad with nuls instead of spaces */
    memset(dest, 0, L_8 + L_3);

    for (len = 0;
	    *source && *source != '/' && *source != CONV_SEP && len < L_8;
	    len++)
	*dest++ = *source++;

    if (source[0] == CONV_SEP && source[1]) {
	int		c;

	source++;
#if CONVERSIONS
	c = source[0] & 31;
	if (c >= ConvFence)
	    c = ConvNone;
	ConversionImbeddedInFileName = c;
#endif /* CONVERSIONS */

	while (*source && *source != '/')
	    source++;
    }
    return source;
#else
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
	byte	       *cp;

	cp = source;
	while (*cp) {
	    if (*cp == '.' || *cp == '/' || *cp == CONV_SEP)
		break;
	    cp++;
	}
	dotp = cp;
	while (*cp) {
	    if (*cp == '/' || *cp == CONV_SEP)
		break;
	    cp++;
	}
	slashp = cp;
    }

    len = dotp - source;
    if (len > L_8)
	len = L_8;

    for (i = 0; i < len; i++) {
	*dest++ = ToUpper(*source++);
    }
    for (; i < L_8; i++) {
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

    if (slashp[0] == CONV_SEP && slashp[1]) {
#if CONVERSIONS
	int		c;
#endif

	slashp++;
#if CONVERSIONS
	c = slashp[0] & 31;
	if (c >= ConvFence)
	    c = ConvNone;
	ConversionImbeddedInFileName = c;
#endif /* CONVERSIONS */

	while (*slashp && *slashp != '/')
	    slashp++;
    }
    return slashp;
#endif
}

#endif /* VFATSUPPORT */

/*
 * Do the Info call.
 */

long
MSDiskInfo(infodata)
struct InfoData *infodata;
{
    setmem(infodata, sizeof(*infodata), 0);

    infodata->id_DiskState = IDDiskState;
    infodata->id_DiskType = IDDiskType;
    infodata->id_UnitNumber = UnitNr;
    debug(("DiskInfo: state %d, type %x\n", IDDiskState, IDDiskType));

    infodata->id_VolumeNode = (BPTR) CTOB(VolNode);
    infodata->id_InUse = LockList ? 1 : 0;

    /*if (IDDiskType == ID_DOS_DISK)*/ {
	infodata->id_NumBlocks = Disk.nsects;
	infodata->id_NumBlocksUsed = Disk.nsects - Disk.freeclusts * Disk.spc;
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
struct LockList **locks;
void	       *cookie;
{
#if HDEBUG
    debug(("MSDiskInserted %08lx\n", cookie));
    if (RootLock)
	debug(("*** RootLock not NULL when new disk inserted !!!\n"));
#endif
    LockList = *locks;
    RootLock = NULL;

    if (LockList == NULL) {
	debug(("MSDiskInserted: no LockList - make fresh one\n"));
	LockList = NewLockList(cookie);
    } else {
	debug(("MSDiskInserted: LockList %p @ %p- RootLock can be duplicated\n", LockList, locks));
	RootLock = MSDupLock(GetTail(&LockList->ll_List));
	/* will be NULL when GetTail() returns NULL */
#if HDEBUG
	if (RootLock == NULL) {
	    debug(("*** RootLock == NULL when taken from LockList !!!\n"));
	}
#endif
    }
    if (RootLock == NULL) {
	RootLock = MakeLock(NULL, &Disk.vollabel, SHARED_LOCK);
    }
#if HDEBUG
    debug(("MSDiskInserted: RootLock = %p\n", RootLock));
    PrintDirEntry((struct DirEntry *)&RootLock->msfl_Msd);
#endif
}

/*
 * Remove the current disk. A place is offered to save the current
 * LockList to restore later. We must unlock the root lock since it isn't
 * a real reference to the disk, just a placeholder for dummies that hand
 * us NULL locks.
 */

int
MSDiskRemoved(locks)
struct LockList **locks;
{
    debug(("MSDiskRemoved(), FatDirty=%d, CacheDirty=%d\n",
	    FatDirty, CacheDirty));
#if !READONLY
    if (FatDirty || CacheDirty) {
	MSUpdate(1);		/* Force a requester */
    }
#endif

    FreeFat();
    FreeCacheList();

    IDDiskType = ID_NO_DISK_PRESENT;
    IDDiskState = ID_WRITE_PROTECTED;
    *locks = NULL;

    if (RootLock == NULL) {
#if HDEBUG
	debug(("*** MSDiskRemoved with no RootLock\n"));
	if (LockList) {
	    void *p;

	    debug(("LockList != NULL: %p\n", LockList));
	    if (p = GetTail(&LockList->ll_List)) {
		debug(("LockList isn't even empty: tail = %p\n", p));
	    }
	}
	/* LockList = NULL;	/ * memory leak */
#endif
	return 1;
    }
#if HDEBUG
    if (RootLock != GetTail(&LockList->ll_List)) {
	debug(("*** RootLock not at end of LockList!\n"));
	/* Get the lock on the root dir at the tail of the List */
	Remove((struct Node *)RootLock);
	AddTail((struct List *)&LockList->ll_List, (struct Node *)RootLock);
    }
#endif

    /*
     * If there are no real locks on the disk, we need not keep any
     * information about it.
     */

    MSUnLock(RootLock); 	/* may call FreeLockList and free VolNode
				 * (!) */
    RootLock = NULL;

    if (LockList) {
	debug(("Storing LockList %p @ %p\n", LockList, locks));
	*locks = LockList;	/* VolNode can't be gone now... */
	LockList = NULL;
	return 0;		/* not all references gone */
    } else {
	debug(("LockList is gone - no locks left on disk\n"));
	return 1;		/* all gone, even the VolNode */
    }
}

#if INPUTDEV
void
InputDiskInserted(void)
{
    struct InputEvent ie;

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_DISKINSERTED;
    ie.ie_Qualifier = IEQUALIFIER_MULTIBROADCAST;
    /*
     * Use Intuition to get the current time.
     * If we were a 2.04+ only application, we could use
     * timer.device/GetSysTime().
     */
    CurrentTime(&ie.ie_TimeStamp.tv_secs, &ie.ie_TimeStamp.tv_micro);

    InputIOReq->io_Command = IND_WRITEEVENT;
    InputIOReq->io_Data = &ie;
    InputIOReq->io_Length = sizeof(ie);
    DoIO((struct IORequest *)InputIOReq);
    debug(("IECLASS_DISKINSERTED %d\n", InputIOReq->io_Error));
}

void
InputDiskRemoved(void)
{
    struct InputEvent ie;

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_DISKREMOVED;
    ie.ie_Qualifier = IEQUALIFIER_MULTIBROADCAST;
    /* Use Intuition to get the current time */
    CurrentTime(&ie.ie_TimeStamp.tv_secs, &ie.ie_TimeStamp.tv_micro);

    InputIOReq->io_Command = IND_WRITEEVENT;
    InputIOReq->io_Data = &ie;
    InputIOReq->io_Length = sizeof(ie);
    DoIO((struct IORequest *)InputIOReq);
    debug(("IECLASS_DISKREMOVED %d\n", InputIOReq->io_Error));
}
#endif

void
HanCloseDown()
{
#if HDEBUG
    struct MSFileLock *fl;

    while (LockList && (fl = (struct MSFileLock *) GetHead(&LockList->ll_List))) {
	debug(("UNLOCKING %08lx: ", fl));
	PrintDirEntry((struct DirEntry *)&fl->msfl_Msd);
	MSUnLock(fl);		/* Remove()s it from this List */
    }
#endif
#if CONVERSIONS
    ConvCleanUp();
#endif
#if INPUTDEV

    if (InputIOReq) {
	if (InputIOReq->io_Unit) {
	    CloseDevice((struct IORequest *)InputIOReq);
	}
	DeleteExtIO((struct IORequest *)InputIOReq);
	InputIOReq = NULL;
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
    IDDiskType = ID_NO_DISK_PRESENT;
    IDDiskState = ID_WRITE_PROTECTED;
    DelayCount = 0;
    Disk.bps = MS_BPS;
    CheckBootBlock = CHECK_BOOTJMP | CHECK_SANITY | CHECK_SAN_DEFAULT;
    InitCacheList();

    TimeIOReq = NULL;

#if HDEBUG
    if (!(DiskReplyPort = CreatePort("MSH:disk.replyport", -1L)))
	goto abort;
#else
    if (!(DiskReplyPort = CreatePort(NULL, 0L)))
	goto abort;
#endif

    debug(("DiskReplyPort = 0x%08lx\n", DiskReplyPort));

    if (!(DiskIOReq = (struct IOExtTD *)
		      CreateExtIO(DiskReplyPort, (long) sizeof(*DiskIOReq)))) {
	debug(("Failed to CreateExtIO\n"));
	goto abort;
    }
    if (OpenDevice(DevName, UnitNr, (struct IORequest *)DiskIOReq,
		   DevFlags | TDF_ALLOW_NON_3_5)) {
	debug(("Failed to OpenDevice\n"));
	goto abort;
    }
    TimeIOReq = (struct timerequest *) CreateExtIO(DiskReplyPort,
					     (long) sizeof(*TimeIOReq));

    if (TimeIOReq == NULL || OpenDevice(TIMERNAME, UNIT_VBLANK,
					(struct IORequest *)TimeIOReq, 0L))
	goto abort;
    TimeIOReq->tr_node.io_Flags = IOF_QUICK;	/* For the first WaitIO() */

#if INPUTDEV
    if (!(InputIOReq = (struct IOStdReq *)CreateExtIO(DiskReplyPort, (long) sizeof(*InputIOReq)))) {
	debug(("Failed to CreateExtIO for input.device\n"));
	goto abort;
    }
    if (OpenDevice("input.device", 0, (struct IORequest *)InputIOReq, 0)) {
	debug(("Failed to Open input.device\n"));
	goto abort;
    }
#endif

    IntuitionBase = OpenLibrary("intuition.library", 0L);   /* cannot fail */
    CheckETDCommands();
    debug(("HanOpenUp() done.\n"));
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
#if READONLY
    return DOSFALSE;
#else
    /*
     * A null or empty string means: remove the label, if any.
     */
    if (!newname || !*newname) {
	if (RootLock->msfl_DirSector != ROOT_SEC) {
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

    if (RootLock->msfl_DirSector == ROOT_SEC) {
	struct MSFileLock *new;

	new = MSLock(RootLock, "OLAF-><>.\\*", EXCLUSIVE_LOCK ^ MODE_CREATEFILE);
	if ((new == NULL) && (new = EmptyFileLock)) {
	    struct DateStamp dateStamp;

	    error = 0;

	    DateStamp(&dateStamp);
	    ToMSDate(&msd_CreationDate(Disk.vollabel.de_Msd),
		     &msd_CreationTime(Disk.vollabel.de_Msd), &dateStamp);

	    Disk.vollabel.de_Msd.msd_ModDate =
		msd_CreationDate(Disk.vollabel.de_Msd);
	    Disk.vollabel.de_Msd.msd_ModTime =
		msd_CreationTime(Disk.vollabel.de_Msd);

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
		struct MsDirEntry *dir;

		fromsec = ReadSec(Disk.rootdir);
		tosec = ReadSec(new->msfl_DirSector);

		dir = (struct MsDirEntry *) fromsec;
		while (dir->msd_Attributes & (ATTR_SYSTEM | ATTR_DIRECTORY)) {
		    if ((byte *) ++dir >= fromsec + Disk.bps) {
			--dir;	/* Back to last entry in the block */
			break;	/* and move it no matter what */
		    }
		}
		CopyMem((byte *)dir, tosec + new->msfl_DirOffset,
			(long) sizeof(struct MsDirEntry));
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
	    int 	    i;
	    byte	   *s,
			   *d;

	    s = newname;
	    d = Disk.vollabel.de_Msd.msd_Name;
	    for (i = 0; i < L_8 + L_3; i++) {
#if LONGNAMES
		if (s[0])
		    *d++ = *s++;
		else
		    *d++ = '\0';
#else
		if (s[0])
		    *d++ = ToUpper(*s++);
		else
		    *d++ = ' ';
#endif
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

struct PrivateInfo *
PrivateInfo()
{
    static struct PrivateInfo info = {
	PRIVATE_REVISION,
	sizeof(struct PrivateInfo),
	RCSId,
	&CheckBootBlock,
	&DefaultConversion,
	&DiskIOReq
#if CONVERSIONS
	,2,	/* == ConvFence - 1 */
	&Table_FromPC, &Table_ToPC,
	&Table_FromST, &Table_ToST
#endif
    };

    return &info;
}

#if HDEBUG

_abort()
{
    HanCloseDown();
    RemTask(NULL);
}

#endif				/* HDEBUG */
