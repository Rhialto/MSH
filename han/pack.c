/*-
 * $Id: pack.c,v 1.30a $
 * $Log:	pack.c,v $
 * Revision 1.30  90/06/04  23:15:58  Rhialto
 * Release 1 Patch 3
 *
 *  Originally:
 *
 *	DOSDEVICE.C	    V1.10   2 November 1987
 *
 *	EXAMPLE DOS DEVICE DRIVER FOR AZTEC.C	PUBLIC DOMAIN.
 *
 *	By Matthew Dillon.
 *
 *  This has been stripped and refilled with messydos code
 *  by Olaf Seibert.
 *
 *  This code is (C) Copyright 1989,1990 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
 *
 *  Please note that we are NOT pure, so if you wish to mount
 *  multiple MSDOS units, you must use different copies of this driver.
 *
 *  This file forms the interface between the actual handler code and all
 *  AmigaDOS requirements. It shields it from ugly stuff like BPTRs, BSTRs,
 *  FileLocks, FileHandles and VolumeNodes (in the form of DeviceLists).
 *  Also, most protection against non-inserted disks is done here.
-*/

#include "dos.h"
#include "han.h"

#ifdef HDEBUG
#   define	debug(x)  dbprintf x
#else
#   define	debug(x)
#endif

/*
 * Since this code might be called several times in a row without being
 * unloaded, you CANNOT ASSUME GLOBALS HAVE BEEN ZERO'D!!  This also goes
 * for any global/static assignments that might be changed by running the
 * code.
 */

PORT	       *DosPort;	/* Our DOS port... */
DEVNODE        *DevNode;	/* Our DOS node.. created by DOS for us */
DEVLIST        *VolNode;	/* Device List structure for our volume
				 * node */

void	       *SysBase;	/* EXEC library base */
DOSLIB	       *DOSBase;	/* DOS library base for debug process */
long		PortMask;	/* The signal mask for our DosPort */
long		WaitMask;	/* The signal mask to wait for */
short		DiskChanged;	/* Set by disk change interrupt */
short		Inhibited;	/* Are we inhibited (ACTION_INHIBIT)? */
long		UnitNr; 	/* From */
char	       *DevName;	/*   the */
ulong		DevFlags;	/*     mountlist */
long		DosType;
PACKET	       *DosPacket;	/* For the SystemRequest pr_WindowPtr */

void ChangeIntHand(), DiskChange();
void NewVolNodeName();

struct Interrupt ChangeInt = {
    { 0 },			/* is_Node */
    0,				/* is_Data */
    ChangeIntHand,		/* is_Code */
};

/*
 * Don't call the entry point main().  This way, if you make a mistake
 * with the compile options you'll get a link error.
 */

void
messydoshandler()
{
    register PACKET *packet;
    MSG 	   *msg;
    byte	    notdone;
    long	    OpenCount;	    /* How many open files/locks there are */

    /*
     * Initialize all global variables.  SysBase MUST be initialized
     * before we can make Exec calls.  AbsExecBase is a library symbol
     * referencing absolute memory location 4.
     */

    SysBase = AbsExecBase;
    DOSBase = OpenLibrary("dos.library", 0L);

#ifdef HDEBUG
    /*
     * Initialize debugging code as soon as possible. Only SysBase and
     * DOSBase are required.
     */

    dbinit();
#endif				/* HDEBUG */

    DosPort = &((struct Process *)FindTask(NULL))->pr_MsgPort;

    WaitPort(DosPort);      /* Get Startup Packet  */
    msg = GetMsg(DosPort);
    packet = (PACKET *) msg->mn_Node.ln_Name;


    DevNode = BTOC(PArg3);
    {
	struct FileSysStartupMsg *fssm;
	ulong *environ;
	ulong Reserved;

	DevName = "messydisk.device";
	UnitNr = 0;
	DevFlags = 0;

	MaxCache = 5;
	BufMemType = MEMF_PUBLIC;
	Disk.nsides = MS_NSIDES;
	Disk.spt = MS_SPT;
	Disk.bps = MS_BPS;
	Disk.lowcyl = 0;
	Reserved = 0;

	if (fssm = (struct FileSysStartupMsg *)BTOC(DevNode->dn_Startup)) {
				    /* Same as BTOC(packet->dp_Arg2) */
	    UnitNr = fssm->fssm_Unit;
	    if (fssm->fssm_Device)
		DevName = (char *)BTOC(fssm->fssm_Device)+1;
	    DevFlags = fssm->fssm_Flags;

	    if (environ = BTOC(fssm->fssm_Environ)) {
		debug(("environ size %ld\n", environ[0]));
#define get(xx,yy)  if (environ[DE_TABLESIZE] >= yy) xx = environ[yy];

		get(MaxCache, DE_NUMBUFFERS);
		get(BufMemType, DE_MEMBUFTYPE);
		get(Disk.nsides, DE_NUMHEADS);
		get(Disk.spt, DE_BLKSPERTRACK);
		get(Disk.bps, DE_SIZEBLOCK);
		Disk.bps *= 4;
		debug(("Disk.bps %d\n", Disk.bps));
		get(Disk.lowcyl, DE_LOWCYL);
		get(Reserved, DE_RESERVEDBLKS);
		get(DosType, DE_DOSTYPE);
#undef get
	    }
	}
	Disk.lowcyl *= (long)MS_BPS * Disk.spt * Disk.nsides;
	Disk.lowcyl += (long)MS_BPS * Reserved;
    }

    if (DOSBase && HanOpenUp()) {
	/*
	 * Loading DevNode->dn_Task causes DOS *NOT* to startup a new
	 * instance of the device driver for every reference.	E.G. if
	 * you were writing a CON: device you would want this field to be
	 * NULL.
	 */

	DevNode->dn_Task = DosPort;

	PRes1 = DOSTRUE;
	PRes2 = 0;
    } else {		    /* couldn't open dos.library  */
	PRes1 = DOSFALSE;
	PRes2 = ERROR_DEVICE_NOT_MOUNTED;   /* no better message available */
	returnpacket(packet);
	goto exit;		/* exit process    */
    }
    returnpacket(packet);

    /* Initialize some more global variables	*/

    PortMask = 1L << DosPort->mp_SigBit;
    VolNode = NULL;
    OpenCount = 0;
    Inhibited = 0;

    /* Get the first real packet       */
    WaitPort(DosPort);
    msg = GetMsg(DosPort);
    notdone = 1;
    WaitMask = PortMask | (1L << DiskReplyPort->mp_SigBit);
    TDAddChangeInt(&ChangeInt);
    DiskInserted(WhichDiskInserted());

    goto entry;

    /*
     * Here begins the endless loop, waiting for requests over our message
     * port and executing them.  Since requests are sent over the message
     * port in our device and volume nodes, we must not use our Process
     * message port for this: this precludes being able to call DOS
     * functions ourselves.
     */

top:
    for (notdone = 1; notdone;) {
	Wait(WaitMask);
	if (DiskChanged)
	    DiskChange();
	while (msg = GetMsg(DosPort)) {
	    byte	    buf[256];	/* Max length of BCPL strings is
					 * 255 + 1 for \0. */

    entry:
	    if (DiskChanged)
		DiskChange();
	    packet = (PACKET *) msg->mn_Node.ln_Name;
	    PRes1 = DOSFALSE;
	    PRes2 = 0;
	    error = 0;
	    debug(("Packet: %3ld %08lx %08lx %08lx %10s\n",
		     PType, PArg1, PArg2, PArg3, typetostr(PType)));

	    DosPacket = packet; 	/* For the System Requesters */
	    switch (PType) {
	    case ACTION_DIE:		/* attempt to die?  */
		notdone = 0;		/* try to die	 */
		break;
	    case ACTION_CURRENT_VOLUME: /* -		      VolNode,UnitNr */
		PRes1 = (long) CTOB(VolNode);
		PRes2 = UnitNr;
		break;
	    case ACTION_LOCATE_OBJECT:	/* Lock,Name,Mode	Lock	     */
		{
		    register struct FileLock *newlock;
		    struct FileLock *lock;
		    struct MSFileLock *msfl;
		    long	    lockmode;

		    lock = BTOC(PArg1);
		    if (CheckRead(lock))
			break;
		    btos(PArg2, buf);
		    if ((lockmode = PArg3) != EXCLUSIVE_LOCK)
			lockmode = SHARED_LOCK;
		    msfl = MSLock(lock ? lock->fl_Key : NULL,
				  buf,
				  lockmode);
		    if (msfl) {
			if (newlock = NewFileLock(msfl, lock)) {
			    newlock->fl_Access = lockmode;
			    PRes1 = (long) CTOB(newlock);
			    OpenCount++;
			} else
			    MSUnLock(msfl);
		    }
		}
		break;
	    case ACTION_RENAME_DISK:	/* BSTR:NewName 	   Bool      */
		if (CheckWrite(NULL))
		    break;
		btos(PArg1, buf);
		buf[31] = '\0';
		if (PRes1 = MSRelabel(buf))
		    NewVolNodeName();
		break;
	    case ACTION_FREE_LOCK:	/* Lock 		   Bool      */
		{
		    struct FileLock *lock;
		    struct MSFileLock *msfl;

		    PRes1 = DOSTRUE;
		    lock = BTOC(PArg1);
		    if (lock == NULL)
			break;

		    msfl = (struct MSFileLock *)lock->fl_Key;
		    FreeFileLock(lock); /* may remove last lock on volume */
		    MSUnLock(msfl);     /* may call MayFreeVolNode */
		    OpenCount--;
		}
		break;
	    case ACTION_DELETE_OBJECT:	/* Lock,Name		Bool	     */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg1);
		    if (CheckWrite(lock))
			break;
		    btos(PArg2, buf);
		    PRes1 = MSDeleteFile(lock ? lock->fl_Key : NULL,
					 buf);
		}
		break;
	    case ACTION_RENAME_OBJECT:	/* SLock,SName,DLock,DName   Bool    */
		{
		    struct FileLock *slock, *dlock;
		    char	     buf2[256];

		    slock = BTOC(PArg1);
		    dlock = BTOC(PArg3);
		    if (CheckWrite(slock) || CheckWrite(dlock))
			break;
		    btos(PArg2, buf);
		    btos(PArg4, buf2);
		    PRes1 = MSRename(slock ? slock->fl_Key : NULL,
				     buf,
				     dlock ? dlock->fl_Key : NULL,
				     buf2);
		}
		break;
	    case ACTION_MORECACHE:	/* #BufsToAdd		   Bool      */
		if ((MaxCache += (short) PArg1) <= 0) {
		    MaxCache = 1;
		} else
		    PRes1 = DOSTRUE;
		debug(("Now %d cache sectors\n", MaxCache));
		break;
	    case ACTION_COPY_DIR:	/* Lock 		   Lock      */
		{
		    register struct FileLock *newlock;
		    struct FileLock *lock;
		    struct MSFileLock *msfl;

		    lock = BTOC(PArg1);

		    msfl = MSDupLock(lock ? lock->fl_Key : NULL);

		    if (msfl) {
			if (newlock = NewFileLock(msfl, lock)) {
			    newlock->fl_Access =
				lock ? lock->fl_Access : SHARED_LOCK;
			    PRes1 = (long) CTOB(newlock);
			    OpenCount++;
			} else
			    MSUnLock(msfl);
		    }
		}
		break;
	    case ACTION_SET_PROTECT:	/* -,Lock,Name,Mask	   Bool      */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg2);
		    if (CheckWrite(lock))
			break;
		    btos(PArg3, buf);
		    PRes1 = MSSetProtect(lock ? lock->fl_Key : NULL, buf, PArg4);
		}
		break;
	    case ACTION_CREATE_DIR:	/* Lock,Name		Lock	     */
		{
		    register struct FileLock *newlock;
		    struct FileLock *lock;
		    struct MSFileLock *msfl;

		    lock = BTOC(PArg1);
		    if (CheckWrite(lock))
			break;
		    btos(PArg2, buf);

		    msfl = MSCreateDir(lock ? lock->fl_Key : NULL, buf);

		    if (msfl) {
			if (newlock = NewFileLock(msfl, lock)) {
			    newlock->fl_Access = SHARED_LOCK;
			    PRes1 = (long) CTOB(newlock);
			    OpenCount++;
			} else
			    MSUnLock(msfl);
		    }
		}
		break;
	    case ACTION_EXAMINE_OBJECT: /* Lock,Fib	       Bool	     */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg1);
		    if (CheckRead(lock))
			break;
		    PRes1 = MSExamine(lock ? lock->fl_Key : NULL, BTOC(PArg2));
		}
		break;
	    case ACTION_EXAMINE_NEXT:	/* Lock,Fib	       Bool	     */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg1);
		    if (CheckRead(lock))
			break;
		    PRes1 = MSExNext(lock ? lock->fl_Key : NULL, BTOC(PArg2));
		}
		break;
	    case ACTION_DISK_INFO:	/* InfoData	       Bool:TRUE     */
		PRes1 = MSDiskInfo(BTOC(PArg1));
		break;
	    case ACTION_INFO:	/* Lock,InfoData	       Bool:TRUE     */
		if (CheckRead(BTOC(PArg1)))
		    break;
		PRes1 = MSDiskInfo(BTOC(PArg2));
		break;
	    case ACTION_FLUSH:		/* writeout bufs, disk motor off     */
		MSUpdate(1);
		break;
/*	    case ACTION_SET_COMMENT:	/* -,Lock,Name,Comment	   Bool      */
	    case ACTION_PARENT: /* Lock 		       ParentLock    */
		{
		    register struct FileLock *newlock;
		    struct FileLock *lock;
		    struct MSFileLock *msfl;
		    long mode;

		    lock = BTOC(PArg1);

		    msfl = MSParentDir(lock ? lock->fl_Key : NULL);

		    if (msfl) {
			if (newlock = NewFileLock(msfl, lock)) {
			    newlock->fl_Access = SHARED_LOCK;
			    PRes1 = (long) CTOB(newlock);
			    OpenCount++;
			} else
			    MSUnLock(msfl);
		    }
		}
		break;
	    case ACTION_INHIBIT:	/* Bool 		   Bool      */
		if (Inhibited = PArg1) {
		    DiskRemoved();
		} else { /* Fall through to ACTION_DISK_CHANGE: */
	    case ACTION_DISK_CHANGE:	/* ?			   ?	     */
		    DiskChange();
		}
		PRes1 = DOSTRUE;
		break;
	    case ACTION_SET_DATE: /* -,Lock,Name,CPTRDateStamp	   Bool      */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg2);
		    if (CheckWrite(lock))
			break;
		    btos(PArg3, buf);
		    PRes1 = MSSetDate(lock ? lock->fl_Key : NULL,
				      buf,
				      PArg4);
		}
		break;
	    case ACTION_READ:	/* FHArg1,CPTRBuffer,Length	  ActLength  */
		if (CheckRead(NULL)) {
		    PRes1 = -1;
		} else
		    PRes1 = MSRead(PArg1, PArg2, PArg3);
		break;
	    case ACTION_WRITE:	/* FHArg1,CPTRBuffer,Length	  ActLength  */
		if (CheckWrite(NULL)) {
		    PRes1 = -1;
		} else
		    PRes1 = MSWrite(PArg1, PArg2, PArg3);
		break;
	    case ACTION_OPENNEW:	/* FileHandle,Lock,Name    Bool      */
		{
		    struct MSFileHandle *msfh;
		    struct FileHandle *fh;
		    struct FileLock *lock;

		    if (CheckWrite(BTOC(PArg2)))
			break;
	    case ACTION_OPENRW: 	/* FileHandle,Lock,Name    Bool      */
	    case ACTION_OPENOLD:	/* FileHandle,Lock,Name    Bool      */

		    fh = BTOC(PArg1);
		    lock = BTOC(PArg2);
		    if (CheckRead(lock))
			break;
		    btos(PArg3, buf);
		    debug(("'%s' ", buf));
		    msfh = MSOpen(lock ? lock->fl_Key : NULL,
				  buf,
				  PType);
		    if (msfh) {
			fh->fh_Arg1 = (long) msfh;
			PRes1 = DOSTRUE;
			OpenCount++;
		    }
		}
		break;
	    case ACTION_CLOSE:	/* FHArg1			  Bool:TRUE  */
		MSClose(PArg1);
		PRes1 = DOSTRUE;
		OpenCount--;
		break;
	    case ACTION_SEEK:	/* FHArg1,Position,Mode 	 OldPosition */
		if (CheckRead(NULL)) {
		    PRes1 = -1;
		} else
		    PRes1 = MSSeek(PArg1, PArg2, PArg3);
		break;
		/*
		 * A few other packet types which we do not support
		 */
/*	    case ACTION_WAIT_CHAR:	/* Timeout, ticks	   Bool      */
/*	    case ACTION_RAWMODE:	/* Bool(-1:RAW 0:CON)      OldState  */
	    default:
		error = ERROR_ACTION_NOT_KNOWN;
		break;
	    } /* end switch */
	    if (packet) {
		if (error) {
		    debug(("ERR=%d\n", error));
		    PRes2 = error;
		}
#ifdef HDEBUG
		else {
		    debug(("RES=%06lx\n", PRes1));
		}
#endif
		returnpacket(packet);
		DosPacket = NULL;
	    }
#ifdef HDEBUG
	    else {
		debug(("NOREP\n"));
	    }
#endif
	} /* end while (GetMsg()) */

	/*
	 *  Now check for an other cause of events: timer IO.
	 *  Unfortunately we cannot be sure that we always get a signal
	 *  when the timeout has elapsed, since the same message port is
	 *  used for other IO.
	 */
	if (CheckIO(TimeIOReq)) {   /* Timer finished? */
	    debug(("TimeIOReq is finished\n"));
	    if (DelayState != DELAY_OFF) {
		MSUpdate(0);    /* Also may switch off motor */
	    }
	}
    } /* end for (;notdone) */

#ifdef HDEBUG
    debug(("Can we remove ourselves? "));
    Delay(50L);                 /* I wanna even see the debug message! */
#endif				/* HDEBUG */
    Forbid();
    if (OpenCount || packetsqueued()) {
	Permit();
	debug((" ..  not yet!\n"));
	goto top;		/* sorry... can't exit     */
    }
    debug((" .. yes!\n"));

    /*
     * Causes a new process to be created on next reference.
     */

    DevNode->dn_Task = NULL;
    TDRemChangeInt();
    DiskRemoved();
    HanCloseDown();
    debug(("HanCloseDown returned. dbuninit in 2 seconds:\n"));

    /*
     * Remove debug window, closedown, fall of the end of the world.
     */
exit:
#ifdef HDEBUG
    Delay(100L);                /* This is dangerous! */
    dbuninit();
#endif				/* HDEBUG */

#if 1
    UnLoadSeg(DevNode->dn_SegList);     /* This is real fun. We are still */
    DevNode->dn_SegList = NULL; 	/* Forbid()den, fortunately */
#endif

    CloseLibrary(DOSBase);

    /* Fall off the end of the world. Implicit Permit(). */
}

void
ChangeIntHand()
{
/* INDENT OFF */
#asm
    move.l  a6,-(sp)
#endasm
    DiskChanged = 1;
    Signal(DosPort->mp_SigTask, PortMask);
#asm
    move.l  (sp)+,a6
#endasm
/* INDENT ON */
}

/*
 *  Make a new struct FileLock, for DOS use. It is put on a singly linked
 *  list, which is attached to the same VolumeNode the old lock was on.
 *
 *  Also note that we must ALWAYS be prepared to UnLock() or DupLock()
 *  any FileLocks we ever made, even if the volume in question has been
 *  removed and/or inserted into another drive with another FileSystem
 *  handler!
 *
 * DOS makes certain assumptions about LOCKS.	A lock must minimally be a
 * FileLock structure, with additional private information after the
 * FileLock structure.	The longword before the beginning of the structure
 * must contain the length of structure + 4.
 *
 * NOTE!!!!! The workbench does not follow the rules and assumes it can copy
 * lock structures.  This means that if you want to be workbench
 * compatible, your lock structures must be EXACTLY sizeof(struct
 * FileLock). Also, it sometimes uses uninitialized values for the lock mode...
 */

struct FileLock *
NewFileLock(msfl, fl)
struct MSFileLock *msfl;
struct FileLock *fl;
{
    struct FileLock *newlock;
    DEVLIST *volnode;

    if (fl) {
	volnode = BTOC(fl->fl_Volume);
    } else {
	volnode = VolNode;
    }

    if (newlock = dosalloc((ulong)sizeof (*newlock))) {
	newlock->fl_Key = (ulong) msfl;
	newlock->fl_Task = DosPort;
	newlock->fl_Volume = (BPTR) CTOB(volnode);
	Forbid();
	newlock->fl_Link = volnode->dl_LockList;
	volnode->dl_LockList = (BPTR) CTOB(newlock);
	Permit();
    } else
	error = ERROR_NO_FREE_STORE;

    return newlock;
}

/*
 *  This should be called before MSUnLock(), so that it may call
 *  MayFreeVolNode() which then calls FreeVolNode(). A bit tricky,
 *  I'm sorry for that.
 */

long
FreeFileLock(lock)
struct FileLock *lock;
{
    register struct FileLock *fl;
    register struct FileLock **flp;
    DEVLIST	   *volnode;

    volnode = (DEVLIST *)BTOC(lock->fl_Volume);
    flp = (struct FileLock **) &volnode->dl_LockList;
    for (fl = BTOC(*flp); fl && fl != lock; fl = BTOC(fl->fl_Link))
	flp = (struct FileLock **)&fl->fl_Link;

    if (fl == lock) {
	*(BPTR *)flp = fl->fl_Link;
	dosfree(fl);
	return DOSTRUE;
    } else {
	debug(("Huh?? Could not find filelock!\n"));
	return DOSFALSE;
    }
}

/*
 * Create Volume node and add to the device list.   This will
 * cause the WORKBENCH to recognize us as a disk.   If we
 * don't create a Volume node, Wb will not recognize us.
 * However, we are a MESSYDOS: disk, Volume node or not.
 */

DEVLIST        *
NewVolNode(name, date)
struct DateStamp *date;
char *name;
{
    DOSINFO	   *di;
    register DEVLIST *volnode;
    char	   *volname;	    /* This is my volume name */

    di = BTOC(((ROOTNODE *) DOSBase->dl_Root)->rn_Info);

    if (volnode = dosalloc((ulong)sizeof (DEVLIST))) {
	if (volname = dosalloc(32L)) {
	    volname[0] = strlen(name);
	    strcpy(volname + 1, name);      /* Make sure \0 terminated */

	    volnode->dl_Type = DLT_VOLUME;
	    volnode->dl_Task = DosPort;
	    volnode->dl_DiskType = IDDiskType;
	    volnode->dl_Name = CTOB(volname);
	    volnode->dl_VolumeDate = *date;
	    volnode->dl_MSFileLockList = NULL;

	    Forbid();
	    volnode->dl_Next = di->di_DevInfo;
	    di->di_DevInfo = (long) CTOB(volnode);
	    Permit();
	} else {
	    dosfree(volnode);
	    volnode = NULL;
	}
    } else {
	error = ERROR_NO_FREE_STORE;
    }

    return volnode;
}

/*
 *  Get the current VolNode a new name from the volume label.
 */

void
NewVolNodeName()
{
    if (VolNode) {
	register char *volname = BTOC(VolNode->dl_Name);

	strncpy(volname + 1, Disk.vollabel.de_Msd.msd_Name, 8+3);
	volname[1+8+3] = '\0';      /* Make sure \0 terminated */
	ZapSpaces(volname + 1, volname + 1 + 8+3);
	volname[0] = strlen(volname+1);
    }
}

/*
 * Remove Volume entry.  Since DOS uses singly linked lists, we must
 * (ugg) search it manually to find the link before our Volume entry.
 */

void
FreeVolNode(volnode)
DEVLIST        *volnode;
{
    DOSINFO	   *di = BTOC(((ROOTNODE *) DOSBase->dl_Root)->rn_Info);
    register DEVLIST *dl;
    register void  *dlp;

    debug(("FreeVolNode %08lx\n", volnode));

    if (volnode == NULL)
	return;

    dlp = &di->di_DevInfo;
    Forbid();
    for (dl = BTOC(di->di_DevInfo); dl && dl != volnode; dl = BTOC(dl->dl_Next))
	dlp = &dl->dl_Next;
    if (dl == volnode) {
	*(BPTR *) dlp = dl->dl_Next;
	dosfree(BTOC(dl->dl_Name));
	dosfree(dl);
    }
#ifdef HDEBUG
    else {
	debug(("****PANIC: Unable to find volume node\n"));
    }
#endif				/* HDEBUG */
    Permit();

    if (volnode == VolNode)
	VolNode = NULL;
}

/*
 *  This is also called from the real handler when the last lock on a
 *  volume is UnLock()ed, or the last file has been Close()d.
 */

int
MayFreeVolNode(volnode)
DEVLIST *volnode;
{
    if (volnode->dl_LockList == NULL) {
	FreeVolNode(volnode);
	return 1;
    }

    return 0;
}

/*
 *  Our disk has been removed. Save the FileLocks in the dl_LockList,
 *  and let the handler save its MSFileLocks in the dl_MSFileLockList field.
 *  If it has nothing to save, forget about the volume, and return
 *  DOSTRUE.
 *  There is one subtlety that MSDiskRemoved must know about:
 *  If it MSUnLock()s the last lock on the volume, the VolNode is
 *  deleted via FreeLockList().. MayFreeVolNode().. FreeVolNode().
 *  But then there is no place anymore to put NULL in, so that needs
 *  to be done first.
 */

int
DiskRemoved()
{
    debug(("DiskRemoved %08lx\n", VolNode));

    if (VolNode == NULL) {
	IDDiskType = ID_NO_DISK_PRESENT;/* really business of MSDiskRemoved */
	return DOSTRUE;
    }

    VolNode->dl_Task = NULL;
    MSDiskRemoved(&VolNode->dl_MSFileLockList);
    if (VolNode == NULL) {  /* Could happen via MSDiskRemoved() */
	return DOSTRUE;
    }
    VolNode = NULL;
    return DOSFALSE;
}

/*
 *  Reconstruct everything from a Volume node
 */

void
DiskInserted(volnode)
register DEVLIST	*volnode;
{
    debug(("DiskInserted %08lx\n", volnode));

    VolNode = volnode;

    if (volnode) {
	volnode->dl_Task = DosPort;
	MSDiskInserted(&volnode->dl_MSFileLockList, volnode);
	volnode->dl_MSFileLockList = NULL;
    }
}

DEVLIST *
WhichDiskInserted()
{
    char name[34];
    struct DateStamp date;
    register DEVLIST *dl = NULL;

    if (!Inhibited && IdentifyDisk(name, &date) == 0) {
	DOSINFO        *di = BTOC(((ROOTNODE *) DOSBase->dl_Root)->rn_Info);
	byte	       *nodename;
	int		namelen = strlen(name);

	for (dl = BTOC(di->di_DevInfo); dl; dl = BTOC(dl->dl_Next)) {
	    nodename = BTOC(dl->dl_Name);
	    if (nodename[0] != namelen || strncmp(nodename+1,name,namelen))
		continue;
	    if (dl->dl_VolumeDate == date)  /* Non-standard! Structure compare! */
		break;
	}

	name[31] = '\0';
	if (dl == NULL)
	    dl = NewVolNode(name, &date);
    }

    return dl;
}

void
DiskChange()
{
    debug(("DiskChange\n"));
    DiskChanged = 0;
    DiskRemoved();
    DiskInserted(WhichDiskInserted());
}

int
CheckRead(lock)
struct FileLock *lock;
{
    if (lock && BTOC(lock->fl_Volume) != VolNode)
	error = ERROR_DEVICE_NOT_MOUNTED;
    else if (IDDiskType == ID_NO_DISK_PRESENT)
	error = ERROR_NO_DISK;
    else if (IDDiskType != ID_DOS_DISK)
	error = ERROR_NOT_A_DOS_DISK;

    return error;
}

int
CheckWrite(lock)
struct FileLock *lock;
{
    if (lock && BTOC(lock->fl_Volume) != VolNode)
	error = ERROR_DEVICE_NOT_MOUNTED;
    else if (IDDiskType == ID_NO_DISK_PRESENT)
	error = ERROR_NO_DISK;
    else if (IDDiskType != ID_DOS_DISK)
	error = ERROR_NOT_A_DOS_DISK;
    else if (IDDiskState == ID_VALIDATING)
	error = ERROR_DISK_NOT_VALIDATED;
    else if (IDDiskState != ID_VALIDATED)
	error = ERROR_DISK_WRITE_PROTECTED;

    return error;
}

#ifdef HDEBUG
		    /*	DEBUGGING			*/
PORT *Dbport;	    /*	owned by the debug process	*/
PORT *Dback;	    /*	owned by the DOS device driver	*/
short DBEnable;

/*
 *  DEBUGGING CODE.	You cannot make DOS library calls that access other
 *  devices from within a DOS device driver because they use the same
 *  message port as the driver.  If you need to make such calls you must
 *  create a port and construct the DOS messages yourself.  I do not
 *  do this.  To get debugging info out another PROCESS is created to which
 *  debugging messages can be sent.
 */

extern void debugproc();

dbinit()
{
    TASK *task = FindTask(NULL);

    Dback = CreatePort("MSH:Dback", -1L);
    CreateProc("MSH_DB", (long)task->tc_Node.ln_Pri+1, CTOB(debugproc), 4096L);
    WaitPort(Dback);                                /* handshake startup    */
    GetMsg(Dback);                                  /* remove dummy msg     */
    DBEnable = 1;
    dbprintf("Debugger running V1.10\n");
}

dbuninit()
{
    MSG killmsg;

    if (Dbport) {
	killmsg.mn_Length = 0;	    /*	0 means die	    */
	PutMsg(Dbport,  &killmsg);
	WaitPort(Dback);            /*  He's dead jim!      */
	GetMsg(Dback);
	DeletePort(Dback);

	/*
	 *  Since the debug process is running at a greater priority, I
	 *  am pretty sure that it is guarenteed to be completely removed
	 *  before this task gets control again.  Still, it doesn't hurt...
	 */

	Delay(50L);                 /*  ensure he's dead    */
    }
}

dbprintf(a,b,c,d,e,f,g,h,i,j)
long a,b,c,d,e,f,g,h,i,j;
{
    struct {
	MSG	msg;
	char	buf[256];
    } msgbuf;
    register MSG *msg = &msgbuf.msg;
    register long len;

    if (Dbport && DBEnable) {
	sprintf(msgbuf.buf,a,b,c,d,e,f,g,h,i,j);
	len = strlen(msgbuf.buf)+1;
	msg->mn_Length = len;	/*  Length NEVER 0  */
	PutMsg(Dbport, msg);
	WaitPort(Dback);
	GetMsg(Dback);
    }
}

/*
 *  BTW, the DOS library used by debugmain() was actually opened by
 *  the device driver.
 */

debugmain()
{
    register MSG *msg;
    register long len;
    register void *fh;
    void *fh2;
    MSG DummyMsg;

    Dbport = CreatePort("MSH:Dbport", -1L);
    fh = Open("CON:0/10/640/190/FileSystem debug", MODE_NEWFILE);
    fh2 = Open("PAR:", MODE_OLDFILE);
    PutMsg(Dback, &DummyMsg);
    for (;;) {
	WaitPort(Dbport);
	msg = GetMsg(Dbport);
	len = msg->mn_Length;
	if (len == 0)
	    break;
	--len;			    /*	Fix length up	*/
	if (DBEnable & 1)
	    Write(fh, msg+1, len);
	if (DBEnable & 2)
	    Write(fh2, msg+1, len);
	PutMsg(Dback, msg);
    }
    Close(fh);
    Close(fh2);
    DeletePort(Dbport);
    PutMsg(Dback, msg);             /*  Kill handshake  */
}

/*
 *  The assembly tag for the DOS process:  CNOP causes alignment problems
 *  with the Aztec assembler for some reason.  I assume then, that the
 *  alignment is unknown.  Since the BCPL conversion basically zero's the
 *  lower two bits of the address the actual code may start anywhere
 *  within 8 bytes of address (remember the first longword is a segment
 *  pointer and skipped).  Sigh....  (see CreateProc() above).
 */

#asm
	public	_debugproc
	public	_debugmain

	cseg
_debugproc:
	nop
	nop
	nop
	nop
	nop
	movem.l D2-D7/A2-A6,-(sp)
	jsr	_debugmain
	movem.l (sp)+,D2-D7/A2-A6
	rts
#endasm

#endif				/* HDEBUG */
