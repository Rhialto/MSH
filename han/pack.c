/*-
 * $Id: pack.c,v 1.46 91/10/06 18:26:16 Rhialto Rel $
 * $Log:	pack.c,v $
 * Revision 1.46  91/10/06  18:26:16  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.43  91/09/28  01:35:36  Rhialto
 * Changed to newer syslog stuff.
 *
 * Revision 1.42  91/06/13  23:46:21  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:45:09  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.32  90/11/23  23:53:22  Rhialto
 * Prepare for syslog
 *
 * Revision 1.31  90/11/10  02:42:38  Rhialto
 * Patch 3a.
 *
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

#include <amiga.h>
#include <functions.h>
#include <string.h>
#include "han.h"
#include "dos.h"

#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

#define MSFL(something)     ((struct MSFileLock *)(something))
#define MSFH(something)     ((struct MSFileHandle *)(something))

Prototype struct MsgPort *DosPort;
Prototype struct DeviceNode *DevNode;
Prototype struct DeviceList *VolNode;
Prototype short     DiskChanged;
Prototype long	    UnitNr;
Prototype char	   *DevName;
Prototype ulong     DevFlags;
Prototype long	    Interleave;
Prototype struct DosPacket *DosPacket;

Prototype struct DeviceList *NewVolNode(char *name, struct DateStamp *date);
Prototype int	    MayFreeVolNode(struct DeviceList *volnode);
Prototype void	    FreeVolNode(struct DeviceList *volnode);
Prototype struct FileLock *NewFileLock(struct MSFileLock *msfl, struct FileLock *fl);
Prototype long	    FreeFileLock(struct FileLock *lock);
Prototype int	    DiskRemoved(void);
Prototype void	    DiskInserted(struct DeviceList *volnode);
Prototype struct DeviceList *WhichDiskInserted(void);
Prototype int	    CheckRead(struct FileLock *lock);
Prototype int	    CheckWrite(struct FileLock *lock);

__stkargs /*__geta4*/ void ChangeIntHand(void);
__stkargs void ChangeIntHand0(void);
char *rega4(void);
Local void NewVolNodeName(void);
Local void DiskChange(void);
Local BPTR MakeFileLock(struct MSFileLock *msfl, struct FileLock *fl, long mode);

/*
 * Since this code might be called several times in a row without being
 * unloaded, you CANNOT ASSUME GLOBALS HAVE BEEN ZERO'D!!  This also goes
 * for any global/static assignments that might be changed by running the
 * code.
 */

struct MsgPort	*DosPort;	/* Our DOS port... */
struct DeviceNode *DevNode;	/* Our DOS node.. created by DOS for us */
struct DeviceList *VolNode;	/* Device List structure for our volume
				 * node */

struct DosLibrary *DOSBase;	/* DOS library base */
long		PortMask;	/* The signal mask for our DosPort */
long		WaitMask;	/* The signal mask to wait for */
short		DiskChanged;	/* Set by disk change interrupt */
short		Inhibited;	/* Are we inhibited (ACTION_INHIBIT)? */
long		UnitNr; 	/* From */
char	       *DevName;	/*   the */
ulong		DevFlags;	/*     mountlist */
long		Interleave;
struct DosPacket *DosPacket;	/* For the SystemRequest pr_WindowPtr */
long		OpenCount;	/* How many open files/locks there are */
short		WriteProtect;	/* Are we software-writeprotected? */

struct Interrupt ChangeInt = {
    { 0 },			/* is_Node */
    0,				/* is_Data */
    ChangeIntHand0,		 /* is_Code */
};

/*
 * Don't call the entry point main().  This way, if you make a mistake
 * with the compile options you'll get a link error.
 */

void
messydoshandler(void)
{
    register struct DosPacket *packet;
    struct Message *msg;
    short	    done;

    /*
     * Initialize all global variables.  SysBase MUST be initialized
     * before we can make Exec calls.  AbsExecBase is a library symbol
     * referencing absolute memory location 4.
     */

    /* SysBase = AbsExecBase; */
    DOSBase = OpenLibrary("dos.library", 0L);

#ifdef HDEBUG
    /*
     * Initialize debugging code as soon as possible. Only SysBase required.
     */

    initsyslog();
#endif				/* HDEBUG */

    DosPort = &((struct Process *)FindTask(NULL))->pr_MsgPort;

    WaitPort(DosPort);      /* Get Startup Packet  */
    msg = GetMsg(DosPort);
    packet = (struct DosPacket *) msg->mn_Node.ln_Name;


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
	Interleave = 0;

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
		debug(("Disk.bps %ld\n", (long)Disk.bps));
		get(Disk.lowcyl, DE_LOWCYL);
		get(Reserved, DE_RESERVEDBLKS);
		/* Compatibility with old DosType = 1 */
		get(Interleave, DE_DOSTYPE);
		if (Interleave == 1) {
		    Interleave = NICE_TO_DFx;
		} else {
		    get(Interleave, DE_INTERLEAVE) else Interleave = 0;
		}
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
    done = -1;
    WaitMask = PortMask | (1L << DiskReplyPort->mp_SigBit);
    ChangeInt.is_Data = rega4();        /* for PURE code */
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
    for (done = -1; done < 0;) {
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
		done = (PArg1 == 'Msh\0') ? PArg2 : 0;   /* Argh! Hack! */
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
		    btos((byte *)PArg2, buf);
		    if ((lockmode = PArg3) != EXCLUSIVE_LOCK)
			lockmode = SHARED_LOCK;
		    msfl = MSLock(MSFL(lock ? lock->fl_Key : NULL),
				  buf,
				  lockmode);
		    PRes1 = MakeFileLock(msfl, lock, lockmode);
		}
		break;
	    case ACTION_RENAME_DISK:	/* BSTR:NewName 	   Bool      */
		if (CheckWrite(NULL))
		    break;
		btos((byte *)PArg1, buf);
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

		    msfl = MSFL(lock->fl_Key);
		    FreeFileLock(lock); /* may remove last lock on volume */
		    MSUnLock(msfl);     /* may call MayFreeVolNode */
		}
		break;
	    case ACTION_DELETE_OBJECT:	/* Lock,Name		Bool	     */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg1);
		    if (CheckWrite(lock))
			break;
		    btos((byte *)PArg2, buf);
		    PRes1 = MSDeleteFile(MSFL(lock ? lock->fl_Key : NULL),
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
		    btos((byte *)PArg2, buf);
		    btos((byte *)PArg4, buf2);
		    PRes1 = MSRename(MSFL(slock ? slock->fl_Key : NULL),
				     buf,
				     MSFL(dlock ? dlock->fl_Key : NULL),
				     buf2);
		}
		break;
	    case ACTION_MORECACHE:	/* #BufsToAdd		   Bool      */
		if ((MaxCache += (short) PArg1) <= 0) {
		    MaxCache = 1;
		} else
		    PRes1 = DOSTRUE;
		debug(("Now %ld cache sectors\n", (long)MaxCache));
		break;
	    case ACTION_COPY_DIR:	/* Lock 		   Lock      */
		{
		    struct FileLock *lock;
		    struct MSFileLock *msfl;

		    lock = BTOC(PArg1);

		    msfl = MSDupLock(MSFL(lock ? lock->fl_Key : NULL));

		    PRes1 = MakeFileLock(msfl, lock,
					 lock ? lock->fl_Access : SHARED_LOCK);
		}
		break;
	    case ACTION_SET_PROTECT:	/* -,Lock,Name,Mask	   Bool      */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg2);
		    if (CheckWrite(lock))
			break;
		    btos((byte *)PArg3, buf);
		    PRes1 = MSSetProtect(MSFL(lock ? lock->fl_Key : NULL),
					buf, PArg4);
		}
		break;
	    case ACTION_CREATE_DIR:	/* Lock,Name		Lock	     */
		{
		    struct FileLock *lock;
		    struct MSFileLock *msfl;

		    lock = BTOC(PArg1);
		    if (CheckWrite(lock))
			break;
		    btos((byte *)PArg2, buf);

		    msfl = MSCreateDir(MSFL(lock ? lock->fl_Key : NULL),
				       buf);

		    PRes1 = MakeFileLock(msfl, lock, SHARED_LOCK);
		}
		break;
	    case ACTION_EXAMINE_OBJECT: /* Lock,Fib	       Bool	     */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg1);
		    /*
		    if (CheckRead(lock))
			break;
		    */
		    PRes1 = MSExamine(MSFL(lock ? lock->fl_Key : NULL),
				      BTOC(PArg2));
		}
		break;
	    case ACTION_EXAMINE_NEXT:	/* Lock,Fib	       Bool	     */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg1);
		    if (CheckRead(lock))
			break;
		    PRes1 = MSExNext(MSFL(lock ? lock->fl_Key : NULL), BTOC(PArg2));
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
		    struct FileLock *lock;
		    struct MSFileLock *msfl;

		    lock = BTOC(PArg1);

		    msfl = MSParentDir(MSFL(lock ? lock->fl_Key : NULL));

		    PRes1 = MakeFileLock(msfl, lock, SHARED_LOCK);
		}
		break;
	    case ACTION_INHIBIT:	/* Bool 		   Bool      */
		if (PArg1) {
		    if (++Inhibited == 1)
			DiskRemoved();
		} else {
		    if (--Inhibited <= 0) {
			Inhibited = 0;
			DiskChange();
		    }
		}
		PRes1 = DOSTRUE;
		break;
	    case ACTION_SET_DATE: /* -,Lock,Name,CPTRDateStamp	   Bool      */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg2);
		    if (CheckWrite(lock))
			break;
		    btos((byte *)PArg3, buf);
		    PRes1 = MSSetDate(MSFL(lock ? lock->fl_Key : NULL),
				      buf,
				      (struct DateStamp *)PArg4);
		}
		break;
#ifdef ACTION_SAME_LOCK
	    case ACTION_SAME_LOCK:  /* Lock1,Lock2		   Result    */
		{
		    struct FileLock *fl1, *fl2;

		    fl1 = BTOC(PArg1);
		    fl2 = BTOC(PArg2);
		    if (fl1->fl_Task == fl2->fl_Task) {
			PRes1 = MSSameLock(MSFL(fl1->fl_Key),
					   MSFL(fl2->fl_Key));
		    } else {
			PRes1 = LOCK_DIFFERENT;
			error = ERROR_DEVICE_NOT_MOUNTED;
		    }
		}
		break;
#endif
	    case ACTION_READ:	/* FHArg1,CPTRBuffer,Length	  ActLength  */
		if (CheckLock(MSFH(PArg1)->msfh_FileLock) ||
		    CheckRead(NULL)) {
		    PRes1 = -1;
		} else
		    PRes1 = MSRead(MSFH(PArg1), (byte *)PArg2, PArg3);
		break;
	    case ACTION_WRITE:	/* FHArg1,CPTRBuffer,Length	  ActLength  */
		if (CheckLock(MSFH(PArg1)->msfh_FileLock) ||
		    CheckWrite(NULL)) {
		    PRes1 = -1;
		} else
		    PRes1 = MSWrite(MSFH(PArg1), (byte *)PArg2, PArg3);
		break;
	    case ACTION_OPENRW: 	/* FileHandle,Lock,Name    Bool      */
	    case ACTION_OPENOLD:	/* FileHandle,Lock,Name    Bool      */
		goto open_notnew;
	    case ACTION_OPENNEW:	/* FileHandle,Lock,Name    Bool      */
		{
		    struct MSFileHandle *msfh;
		    struct FileHandle *fh;
		    struct FileLock *lock;

		    if (CheckWrite(BTOC(PArg2)))
			break;

		open_notnew:
		    fh = BTOC(PArg1);
		    lock = BTOC(PArg2);
		    if (CheckRead(lock))
			break;
		    btos((byte *)PArg3, buf);
		    debug(("'%s' ", buf));
		    msfh = MSOpen(MSFL(lock ? lock->fl_Key : NULL),
				  buf,
				  PType);
		    if (msfh) {
			fh->fh_Arg1 = (long) msfh;
			PRes1 = DOSTRUE;
			OpenCount++;
		    }
		}
		break;
	    case ACTION_CLOSE:	/* FHArg1			Bool:Success */
		PRes1 = MSClose(MSFH(PArg1));
		OpenCount--;
		break;
	    case ACTION_SEEK:	/* FHArg1,Position,Mode 	 OldPosition */
		if (CheckLock(MSFH(PArg1)->msfh_FileLock) ||
		    CheckRead(NULL)) {
		    PRes1 = -1;
		} else
		    PRes1 = MSSeek(MSFH(PArg1), PArg2, PArg3);
		break;
#ifdef ACTION_SET_FILE_SIZE
	    case ACTION_SET_FILE_SIZE:
		PRes1 = MSSetFileSize(
			    MSFH(((struct FileHandle *)BTOC(PArg1))->fh_Arg1),
			    PArg2, PArg3);
		break;
#endif
#ifdef ACTION_WRITE_PROTECT
	    case ACTION_WRITE_PROTECT:
		{
		    static long     Passkey;

		    if (PArg1) {
			if (Passkey == 0) {
			    WriteProtect = 1;
			    Passkey = PArg2;
			    PRes1 = DOSTRUE;
			}
		    } else {
			if (Passkey == 0 || PArg2 == Passkey) {
			    WriteProtect = 0;
			    Passkey = 0;
			    PRes1 = DOSTRUE;
			}
		    }
		}
		break;
#endif
#ifdef ACTION_FH_FROM_LOCK	/* FH,Lock			    BOOL     */
	    case ACTION_FH_FROM_LOCK:
		{
		    struct MSFileHandle *msfh;
		    struct FileHandle *fh;
		    struct FileLock *lock;

		    fh = BTOC(PArg1);
		    lock = BTOC(PArg2);
		    if (CheckRead(lock))
			break;
		    msfh = MSOpenFromLock(MSFL(lock ? lock->fl_Key : NULL));
		    if (msfh) {
			fh->fh_Arg1 = (long) msfh;
			PRes1 = DOSTRUE;
			OpenCount++;
			/* Discard the lock */
			FreeFileLock(lock);
		    }
		}
		break;
#endif
#ifdef ACTION_IS_FILESYSTEM
		case ACTION_IS_FILESYSTEM:
		    PRes1 = DOSTRUE;
		    break;
#endif
#ifdef ACTION_CHANGE_MODE
		case ACTION_CHANGE_MODE:
		    switch (PArg1) {
		    case CHANGE_FH:
			PRes1 = MSChangeModeFH(MSFH(((struct FileHandle *)BTOC(PArg2))->fh_Arg1), PArg3);
			break;
		    case CHANGE_LOCK:
			PRes1 = MSChangeModeLock(MSFL(((struct FileLock *)BTOC(PArg2))->fh_Key), PArg3);
			break;
		    }
		    break;
#endif
#ifdef ACTION_COPY_DIR_FH
	    case ACTION_COPY_DIR_FH:	/* FH			   Lock      */
	    case ACTION_PARENT_FH:
		{
		    struct FileHandle *fh;
		    struct MSFileLock *msfl;

		    fh = BTOC(PArg1);
		    /*
		    if (PType == ACTION_PARENT_FH)
			msfl = MSParentOfFH(MSFH(fh->fh_Arg1));
		    else
			msfl = MSDupLockFromFH(MSFH(fh->fh_Arg1));
		    */
		    msfl = ((PType == ACTION_PARENT_FH) ?
			    MSParentOfFH : MSDupLockFromFH) (MSFH(fh->fh_Arg1));

		    PRes1 = MakeFileLock(msfl, lock, SHARED_LOCK);
		}
		break;
#endif
#ifdef ACTION_EXAMINE_FH
	    case ACTION_EXAMINE_FH:	 /* FH,Fib		   Bool      */
		{
		    struct FileHandle *fh;

		    fh = BTOC(PArg1);
		    PRes1 = MSExamineFH(MSFH(fh->fh_Arg1), BTOC(PArg2));
		}
		break;
#endif
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
		    debug(("ERR=%ld\n", (long)error));
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
	if (CheckIO(&TimeIOReq->tr_node)) {   /* Timer finished? */
	    debug(("TimeIOReq is finished\n"));
	    if (DelayState != DELAY_OFF) {
		MSUpdate(0);    /* Also may switch off motor */
	    }
	}
    } /* end for (;done) */

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
    debug(("HanCloseDown returned. uninitsyslog in 2 seconds:\n"));

    /*
     * Remove debug, closedown, fall of the end of the world.
     */
exit:
#ifdef HDEBUG
    Delay(100L);                /* This is dangerous! */
    uninitsyslog();
#endif				/* HDEBUG */

    if (done & 2)
	UnLoadSeg(DevNode->dn_SegList); /* This is real fun. We are still */
    if (done & (2 | 1))
	DevNode->dn_SegList = NULL;	/* Forbid()den, fortunately */

    CloseLibrary(DOSBase);

    /* Fall off the end of the world. Implicit Permit(). */
}

/*
 * ChangeIntHand must be __geta4 if it is called directly through the
 * ChangeInt.is_Code pointer, but then we can't be pure.
 * If ChangeIntHand0 is called, __geta4 is done there via is_Data.
 */

__stkargs /*__geta4*/ void
ChangeIntHand(void)
{
    DiskChanged = 1;
    Signal(DosPort->mp_SigTask, PortMask);
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
    struct DeviceList *volnode = NULL;

    if (fl) {
	volnode = BTOC(fl->fl_Volume);
    }
    if (volnode == NULL) {
	volnode = VolNode;
	debug(("volnode 0->%lx\n", volnode));
    }
#ifdef HDEBUG
    if (volnode != VolNode) {
	debug(("volnode != VolNode %lx != %lx\n",
	    volnode, VolNode));
    }
    if (volnode->dl_Task != DosPort) {
	debug(("volnode->dl_Task != DosPort %lx != %lx\n",
	    volnode->dl_Task, DosPort));
    }
#endif

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
    struct DeviceList	     *volnode;

    volnode = (struct DeviceList *)BTOC(lock->fl_Volume);
    flp = (struct FileLock **) &volnode->dl_LockList;
    for (fl = BTOC(*flp); fl && fl != lock; fl = BTOC(fl->fl_Link))
	flp = (struct FileLock **)&fl->fl_Link;

    if (fl == lock) {
	*(BPTR *)flp = fl->fl_Link;
	dosfree((ulong *)fl);
	OpenCount--;
	return DOSTRUE;
    } else {
	debug(("Huh?? Could not find filelock!\n"));
	return DOSFALSE;
    }
}

/*
 * MakeFileLock allocates and initializes a new FileLock, using info
 * from an existing FileLock. It always consumes the MSFileLock, even
 * in case of failure.
 */

BPTR
MakeFileLock(msfl, fl, mode)
struct MSFileLock *msfl;
struct FileLock *fl;
long		mode;
{
    struct FileLock *newlock;

    newlock = NULL;
    if (msfl) {
	if (newlock = NewFileLock(msfl, fl)) {
	    newlock->fl_Access = mode;
	    OpenCount++;
	} else
	    MSUnLock(msfl);
    }

    return CTOB(newlock);
}

/*
 * Create Volume node and add to the device list.   This will
 * cause the WORKBENCH to recognize us as a disk.   If we
 * don't create a Volume node, Wb will not recognize us.
 * However, we are a MESSYDOS: disk, Volume node or not.
 */

struct DeviceList	 *
NewVolNode(name, date)
char *name;
struct DateStamp *date;
{
    struct DosInfo *di;
    struct DeviceList *volnode;
    char	   *volname;	    /* This is my volume name */

    di = BTOC(((struct RootNode *) DOSBase->dl_Root)->rn_Info);

    if (volnode = dosalloc((ulong)sizeof (struct DeviceList))) {
	if (volname = dosalloc(32L)) {
	    volname[0] = strlen(name);
	    strcpy(volname + 1, name);      /* Make sure \0 terminated */

	    volnode->dl_Type = DLT_VOLUME;
	    volnode->dl_Task = DosPort;
	    volnode->dl_DiskType = IDDiskType;
	    volnode->dl_Name = (BSTR *)CTOB(volname);
	    volnode->dl_VolumeDate = *date;
	    volnode->dl_MSFileLockList = NULL;

	    Forbid();
	    volnode->dl_Next = di->di_DevInfo;
	    di->di_DevInfo = (long) CTOB(volnode);
	    Permit();
	} else {
	    dosfree((ulong *)volnode);
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
struct DeviceList *volnode;
{
    struct DosInfo *di = BTOC(((struct RootNode *) DOSBase->dl_Root)->rn_Info);
    register struct DeviceList *dl;
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
	dosfree((ulong *)dl);
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
struct DeviceList *volnode;
{
    if (volnode->dl_LockList == NULL) {
	FreeVolNode(volnode);
	return 1;
    }

    return 0;
}

#ifdef PROMISE_NOT_TO_DIE
/*
 * This function makes it possible to transfer locks from one handler to
 * another, when the disk is being moved from one drive to another. The
 * Amiga file system also does it. Try doing a "list df0:", pause it, put
 * the disk in df1:, and continue. Unfortunately, passing FileLocks between
 * handlers assumes that they will stick around forever. Therefore we make
 * the user promise this by setting the PROMISE_NOT_TO_DIE flag.
 */

void RedirectLocks(struct DeviceList *volnode);
void
RedirectLocks(volnode)
struct DeviceList *volnode;
{
    if (Interleave & PROMISE_NOT_TO_DIE) {
	struct FileLock *fl;

	fl = BTOC(volnode->dl_LockList);
	while (fl) {
	    /*
	     * This is no good for their OpenCount...
	     * they can never die again!
	     */
	    fl->fl_Task = volnode->dl_Task;
	    OpenCount++;
	    fl = BTOC(fl->fl_Link);
	}
    }
}

#endif

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
struct DeviceList *volnode;
{
    debug(("DiskInserted %08lx\n", volnode));

    VolNode = volnode;

    if (volnode) {
	volnode->dl_Task = DosPort;
#ifdef PROMISE_NOT_TO_DIE
	RedirectLocks(volnode);
#endif
	MSDiskInserted(&volnode->dl_MSFileLockList, volnode);
	volnode->dl_MSFileLockList = NULL;
    }
}

struct DeviceList *
WhichDiskInserted()
{
    char name[34];
    struct DateStamp date;
    register struct DeviceList *dl = NULL;

    if (!Inhibited && IdentifyDisk(name, &date) == 0) {
	struct DosInfo *di = BTOC(((struct RootNode *) DOSBase->dl_Root)->rn_Info);
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
    if (CheckRead(lock))
	/* nothing */ ;
    else if (IDDiskState == ID_VALIDATING)
	error = ERROR_DISK_NOT_VALIDATED;
    else if (IDDiskState != ID_VALIDATED || WriteProtect)
	error = ERROR_DISK_WRITE_PROTECTED;

    return error;
}
