/*-
 * $Id: pack.c,v 1.58 2005/10/19 16:53:52 Rhialto Exp $
 * $Log: pack.c,v $
 * Revision 1.58  2005/10/19  16:53:52  Rhialto
 * Finally a new version!
 *
 * Revision 1.56  1996/12/22  00:22:33  Rhialto
 * Better (but not perfect) device list deadlock handling.
 * Add some Guru Book packets.
 * Pretend success on ACTION_SET_COMMENT.
 * Support for proper taskwait hook.
 *
 * Revision 1.55  1993/12/30  23:02:45	Rhialto
 * Add code to reflect changing disk capacity to Mount info.
 * Don't fail Info() if there is no disk in drive.
 * SameLock() was wrong because TADM was wrong.
 * Add Format() packet.
 * Use 2.0+ calls for manipulating the device list, if possible.
 * New LONGNAMES filesystem, changes throughout the handler.
 * Optional (compile-time) broadcast IECLASS_DISKINSERTED messages.
 * Freeze for MAXON5.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:23:50  Rhialto
 * Add 2.0 stuff.
 *
 * Revision 1.51  92/04/17  15:34:59  Rhialto
 * Freeze for MAXON. DosType->Interleave; extra uninhibits fixed.
 *
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
 *  This code is (C) Copyright 1989-1992 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
 *
 *  This file forms the interface between the actual handler code and all
 *  AmigaDOS requirements. It shields it from ugly stuff like BPTRs, BSTRs,
 *  FileLocks, FileHandles and VolumeNodes (in the form of DeviceLists).
 *  Also, most protection against non-inserted disks is done here.
-*/

#include "han.h"
#include "dos.h"
#include <string.h>

#if HDEBUG
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
Prototype struct DosEnvec *Environ;
Prototype struct DosPacket *DosPacket;
Prototype short     Inhibited;
Prototype byte     *StackBottom;

Prototype struct DeviceList *NewVolNode(char *name, struct DateStamp *date);
Prototype int	    MayFreeVolNode(struct DeviceList *volnode);
Prototype void	    FreeVolNode(struct DeviceList *volnode);
Prototype void	    FreeVolNodeDeferred(void);
Prototype struct FileLock *NewFileLock(struct MSFileLock *msfl, struct FileLock *fl);
Prototype long	    FreeFileLock(struct FileLock *lock);
Prototype long	    DiskRemoved(void);
Prototype void	    DiskInserted(struct DeviceList *volnode);
Prototype void	    CheckDriveType(void);
Prototype struct DeviceList *WhichDiskInserted(void);
Prototype void	    DiskChange(void);
Prototype int	    CheckRead(struct FileLock *lock);
Prototype int	    CheckWrite(struct FileLock *lock);

__stkargs /*__geta4*/ void ChangeIntHand(void);
__stkargs void ChangeIntHand0(void);
char *rega4(void);
Local void NewVolNodeName(void);
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
struct DeviceList *MustFreeVolNode; /* Deferred free. Just room for one. */

struct DosLibrary *DOSBase;	/* DOS library base */
long		PortMask;	/* The signal mask for our DosPort */
long		WaitMask;	/* The signal mask to wait for */
short		DiskChanged;	/* Set by disk change interrupt */
short		Inhibited;	/* Are we inhibited (ACTION_INHIBIT)? */
long		UnitNr; 	/* From */
char	       *DevName;	/*   the */
ulong		DevFlags;	/*     mountlist */
long		Interleave;
struct DosEnvec *Environ;
struct DosPacket *DosPacket;	/* For the SystemRequest pr_WindowPtr */
long		OpenCount;	/* How many open files/locks/other
				 * references there are */
short		WriteProtect;	/* Are we software-writeprotected? */
byte	       *StackBottom;
char		MessydiskDevice[] = "messydisk.device";

struct Interrupt ChangeInt = {
    { 0 },			/* is_Node */
    0,				/* is_Data */
    ChangeIntHand0		/* is_Code */
};

/*
 * Don't call the entry point main().  This way, if you make a mistake
 * with the compile options you'll get a link error.
 */

void
messydoshandler(void)
{
    struct DosPacket *packet;
#if ! TASKWAIT
    struct Message *msg;
#endif
    struct Process *myproc;
    short	    done;
    struct FileSysStartupMsg *fssm;

    /*
     * Initialize all global variables.  SysBase MUST be initialized
     * before we can make Exec calls.  AbsExecBase is a library symbol
     * referencing absolute memory location 4.
     */

    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0L);

#if HDEBUG
    /*
     * Initialize debugging code as soon as possible. Only SysBase required.
     */

    initsyslog();
#endif				/* HDEBUG */

    myproc = (struct Process *)FindTask(NULL);
    DosPort = &myproc->pr_MsgPort;
    StackBottom = myproc->pr_Task.tc_SPLower;

#if TASKWAIT
    packet = taskwait(myproc);
#else
    WaitPort(DosPort);	    /* Get Startup Packet  */
    msg = GetMsg(DosPort);
    packet = (struct DosPacket *) msg->mn_Node.ln_Name;
#endif

    DevNode = BTOC(PArg3);
    {
	ulong Reserved;

	DevName = MessydiskDevice;
	UnitNr = 0;
	DevFlags = 0;

	MaxCache = INITIAL_MAX_CACHE;
	BufMemType = MEMF_PUBLIC;
	DefaultDisk.nsides = MS_NSIDES;
	DefaultDisk.spt = MS_SPT;
	DefaultDisk.bps = MS_BPS;
	Partition.offset = 0;
	Reserved = 0;
	Interleave = 0;

	if (fssm = (struct FileSysStartupMsg *)BTOC(DevNode->dn_Startup)) {
				    /* Same as BTOC(packet->dp_Arg2) */
	    UnitNr = fssm->fssm_Unit;
	    if (fssm->fssm_Device)
		DevName = (char *)BTOC(fssm->fssm_Device)+1;
	    DevFlags = fssm->fssm_Flags;

	    if (Environ = (void *)BTOC(fssm->fssm_Environ)) {
		debug(("Environ size %ld\n", Environ->de_TableSize));

		if (Environ->de_TableSize >= DE_NUMBUFFERS) {
		    if (Environ->de_NumBuffers > MaxCache)
			MaxCache = Environ->de_NumBuffers;

		    DefaultDisk.nsides = Environ->de_Surfaces;
		    DefaultDisk.spt = Environ->de_BlocksPerTrack;
		    DefaultDisk.bps = Environ->de_SizeBlock * 4;
		    debug(("DefaultDisk.bps %ld\n", (long)DefaultDisk.bps));

		    Partition.offset = Environ->de_LowCyl;
		    Reserved = Environ->de_Reserved;

		    /* Compatibility with old DosType = 1 */
		    /* Interleave = Environ->de_DosType;
		    if (Interleave == 1) {
			Interleave = OPT_NICE_TO_DFx;
		    } else */ {
			Interleave = Environ->de_Interleave;
		    }
		    if (Interleave & OPT_NO_WIN95)
			Interleave |= OPT_SHORTNAME;
#if 0
		    /* Auto-detect when not to be nice to DFx: */
		    if (strncmp(Device, MessydiskDevice,
						sizeof(MessydiskDevice)) != 0)
			Interleave &= ~OPT_NICE_TO_DFx;
#endif
#define get(xx,yy)  if (Environ->de_TableSize >= yy) xx = ((ULONG*)Environ)[yy];
		    get(BufMemType, DE_MEMBUFTYPE);
		} else
		    Environ = NULL;
#undef get
	    }
	}
	Disk = DefaultDisk;

	Partition.offset *= Disk.bps * Disk.spt * Disk.nsides;
	Partition.offset += Disk.bps * Reserved;

	/* These values are used only for floppies (i.e. DRIVE*) */
	if (Disk.spt <= MS_SPT_MAX_DD) {
	    Partition.spt_dd = Disk.spt;
	    Partition.spt_hd = Disk.spt * 2;
	} else {
	    Partition.spt_hd = Disk.spt;
	    Partition.spt_dd = Disk.spt / 2;
	}

	debug(("offset %x, spt_dd %d, spt_hd %d\n",
	       Partition.offset, Partition.spt_dd, Partition.spt_hd));
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
    debug(("Returning startup packet %lx\n", packet));
    returnpacket(packet);

    /* Initialize some more global variables	*/

    PortMask = 1L << DosPort->mp_SigBit;
    VolNode = NULL;
    WaitMask = PortMask | (1L << DiskReplyPort->mp_SigBit);
    ChangeInt.is_Data = rega4();	/* for PURE code */
    TDAddChangeInt(&ChangeInt);
    OpenCount = 0;
    Inhibited = 0;

    /* Get the first real packet       */
    debug(("Awaiting first real dos packet\n"));
#if TASKWAIT
    packet = taskwait(myproc);
    debug(("got it: %lx\n", packet));
#else
    WaitPort(DosPort);
    msg = GetMsg(DosPort);
    debug(("got it: %lx\n", msg));
#endif
    done = -1;
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
#if TASKWAIT
	while (packetsqueued()) {	/* } */
#else
	while (msg = GetMsg(DosPort)) {
#endif
	    byte	    buf[256];	/* Max length of BCPL strings is
					 * 255 + 1 for \0. */
#if TASKWAIT
	    packet = taskwait(myproc);
#endif
    entry:
	    if (MustFreeVolNode)
		FreeVolNodeDeferred();
	    if (DiskChanged)
		DiskChange();

#if ! TASKWAIT
	    packet = (PACKET *) msg->mn_Node.ln_Name;
#endif
	    PRes1 = DOSFALSE;
	    PRes2 = 0;
	    error = 0;
	    debug(("Packet: %4ld %08lx %08lx %08lx %s\n",
		     PType, PArg1, PArg2, PArg3, typetostr(PType)));

	    DosPacket = packet; 	/* For the System Requesters */
	    switch (PType) {
	    case ACTION_DIE:		/* attempt to die?  */
		done = (PArg1 == MSH_MAGIC) ? PArg2 : 0;   /* Argh! Hack! */
		break;
	    case ACTION_CURRENT_VOLUME: /* fharg1,Magic,Count -> VolNode,UnitNr,Private */
		if (PArg2 == MSH_MAGIC) {
		    PArg2 = (long)PrivateInfo();
		    if (PArg3 > 0) {
			OpenCount++;
		    } else if (PArg3 < 0) {
			OpenCount--;
		    }
		}
		PRes1 = (long) CTOB(VolNode);
		PRes2 = UnitNr;
		break;
	    case ACTION_LOCATE_OBJECT:	/* Lock,Name,Mode	Lock	     */
		{
		    struct FileLock *lock;
		    struct MSFileLock *msfl;
		    long	    lockmode;

		    lock = BTOC(PArg1);
		    if (CheckRead(lock))
			break;
		    btos((byte *)PArg2, buf);
		    if ((lockmode = PArg3) != EXCLUSIVE_LOCK)
			lockmode = SHARED_LOCK;
		    msfl = MSLock(MSFL(lock ? lock->fl_Key : 0),
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
		    MSUnLock(msfl);	/* may call MayFreeVolNode */
		}
		break;
	    case ACTION_DELETE_OBJECT:	/* Lock,Name		Bool	     */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg1);
		    if (CheckWrite(lock))
			break;
		    btos((byte *)PArg2, buf);
		    PRes1 = MSDeleteFile(MSFL(lock ? lock->fl_Key : 0),
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
		    PRes1 = MSRename(MSFL(slock ? slock->fl_Key : 0),
				     buf,
				     MSFL(dlock ? dlock->fl_Key : 0),
				     buf2);
		}
		break;
	    case ACTION_MORECACHE:	/* #BufsToAdd		bool,numbufs */
		if ((MaxCache += (short) PArg1) <= 0) {
		    MaxCache = 1;
		}
		PRes1 = MaxCache;   /* observed behaviour in std filesystem */
		PRes2 = MaxCache;   /* documented behaviour in manual */
		debug(("Now %ld cache sectors\n", (long)MaxCache));
		break;
	    case ACTION_COPY_DIR:	/* Lock 		   Lock      */
		{
		    struct FileLock *lock;
		    struct MSFileLock *msfl;

		    lock = BTOC(PArg1);

		    msfl = MSDupLock(MSFL(lock ? lock->fl_Key : 0));

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
		    PRes1 = MSSetProtect(MSFL(lock ? lock->fl_Key : 0),
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

		    msfl = MSCreateDir(MSFL(lock ? lock->fl_Key : 0),
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
		    PRes1 = MSExamine(MSFL(lock ? lock->fl_Key : 0),
				      BTOC(PArg2));
		}
		break;
	    case ACTION_EXAMINE_NEXT:	/* Lock,Fib	       Bool	     */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg1);
		    if (CheckRead(lock))
			break;
		    PRes1 = MSExNext(MSFL(lock ? lock->fl_Key : 0),
				     BTOC(PArg2));
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
	    case ACTION_SET_COMMENT:	/* -,Lock,Name,Comment	   Bool      */
		/* pretend to succeed so that COPY CLONE does not give up */
		PRes1 = DOSTRUE;
		break;
	    case ACTION_PARENT: /* Lock 		       ParentLock    */
		{
		    struct FileLock *lock;
		    struct MSFileLock *msfl;

		    lock = BTOC(PArg1);

		    msfl = MSParentDir(MSFL(lock ? lock->fl_Key : 0));

		    PRes1 = MakeFileLock(msfl, lock, SHARED_LOCK);
		}
		break;
	    case ACTION_INHIBIT:	/* Bool 		   Bool      */
		if (PArg1) {
		    ++Inhibited;
		    if (Inhibited == 1)
			DiskRemoved();
		    IDDiskType = 'BUSY';/* GURU book p. 443 */
		} else {
		    --Inhibited;
		    if (Inhibited < 0) {
			Inhibited = 0;	/* Do nothing if already uninhibited */
		    } else if (Inhibited == 0) {
			DiskChange();
		    }
		}
		PRes1 = DOSTRUE;
		error = 0;
		break;
	    case ACTION_SET_DATE: /* -,Lock,Name,CPTRDateStamp	   Bool      */
		{
		    struct FileLock *lock;

		    lock = BTOC(PArg2);
		    if (CheckWrite(lock))
			break;
		    btos((byte *)PArg3, buf);
		    PRes1 = MSSetDate(MSFL(lock ? lock->fl_Key : 0),
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
		    if (fl1->fl_Volume == fl2->fl_Volume) {
			PRes1 = MSSameLock(MSFL(fl1->fl_Key),
					   MSFL(fl2->fl_Key));
		    } else {
			PRes1 = DOSFALSE;
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
	    case ACTION_FINDUPDATE:	/* FileHandle,Lock,Name    Bool      */
	    case ACTION_FINDINPUT:	/* FileHandle,Lock,Name    Bool      */
		goto open_notnew;
	    case ACTION_FINDOUTPUT:	/* FileHandle,Lock,Name    Bool      */
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
		    msfh = MSOpen(MSFL(lock ? lock->fl_Key : 0),
				  buf,
				  PType);
		    if (msfh) {
			fh->fh_Arg1 = (long) msfh;
			PRes1 = DOSTRUE;
			OpenCount++;
		    }
		}
		break;
	    case ACTION_END:	/* FHArg1			Bool:Success */
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
	    case ACTION_FORMAT: /* vol,type			Bool:success */
		btos((byte *)PArg1, buf);
		PRes1 = MSFormat(buf, PArg2);
		break;
#ifdef ACTION_SET_FILE_SIZE	/* FHArg1, off, whence		Bool:success */
	    case ACTION_SET_FILE_SIZE:
		PRes1 = MSSetFileSize(MSFH(PArg1), PArg2, PArg3);
		break;
#endif
#ifdef ACTION_WRITE_PROTECT	/* Bool:protect, passkey	Bool:success */
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
		    msfh = MSOpenFromLock(MSFL(lock ? lock->fl_Key : 0));
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
	    case ACTION_IS_FILESYSTEM:	/* -			   Bool:TRUE */
		PRes1 = DOSTRUE;
		break;
#endif
#ifdef ACTION_CHANGE_MODE
	    case ACTION_CHANGE_MODE:
		switch (PArg1) {
		case CHANGE_FH:
		    PRes1 = MSChangeModeFH(MSFH(((struct FileHandle *)
				BTOC(PArg2))->fh_Arg1), PArg3);
		    break;
		case CHANGE_LOCK:
		    PRes1 = MSChangeModeLock(MSFL(((struct FileLock *)
				BTOC(PArg2))->fl_Key), PArg3);
		    break;
		}
		break;
#endif
#ifdef ACTION_COPY_DIR_FH
	    case ACTION_COPY_DIR_FH:	/* fh_Arg1		   Lock      */
	    case ACTION_PARENT_FH:
		{
		    struct MSFileLock *msfl;

		    if (PType == ACTION_PARENT_FH)
			msfl = MSParentOfFH(MSFH(PArg1));
		    else
			msfl = MSDupLockFromFH(MSFH(PArg1));
		    /*
		    msfl = ((PType == ACTION_PARENT_FH) ?
			    MSParentOfFH : MSDupLockFromFH) (MSFH(PArg1));
		    */

		    /* User has inserted disk by now, so we can use VolNode */
		    PRes1 = MakeFileLock(msfl, NULL, SHARED_LOCK);
		}
		break;
#endif
#ifdef ACTION_EXAMINE_FH
	    case ACTION_EXAMINE_FH:	 /* fh_Arg1,Fib 		Bool	  */
		PRes1 = MSExamineFH(MSFH(PArg1), BTOC(PArg2));
		break;
#endif
	    /* These packets by suggestion of the Amiga Guru Book: */
#if defined(ACTION_SERIALIZE_DISK)
	    case ACTION_SERIALIZE_DISK:
		PRes1 = MSSerializeDisk();
		break;
#endif
#if defined(ACTION_GET_DISK_FSSM)
	    case ACTION_GET_DISK_FSSM:
		PRes1 = (ULONG)fssm;
		OpenCount++;
		break;
	    case ACTION_FREE_DISK_FSSM:
		PRes1 = DOSTRUE;
		OpenCount--;
		break;
#endif
		/*
		 * A few other packet types which we do not support
		 */
/*	    case ACTION_WAIT_CHAR:     / * Timeout, ticks	   Bool      */
/*	    case ACTION_RAWMODE:       / * Bool(-1:RAW 0:CON)	   OldState  */
	    default:
		PRes1 = DOSFALSE;
		error = ERROR_ACTION_NOT_KNOWN;
		break;
	    } /* end switch */
	    if (packet) {
		if (error)
		    PRes2 = error;
		debug(("RES=%06lx, ERR=%ld\n", PRes1, error));
		returnpacket(packet);
		DosPacket = NULL;
	    }
#if HDEBUG
	    else {
		debug(("NO REPLY\n"));
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
	    MSUpdate(0);	/* Also may switch off motor */
	}
    } /* end for (;done) */

#if HDEBUG
    debug(("Can we remove ourselves? "));
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
    FreeVolNodeDeferred();	/* just in case */
    HanCloseDown();
    debug(("HanCloseDown returned. uninitsyslog in 2 seconds:\n"));

    /*
     * Remove debug, closedown, fall of the end of the world.
     */
exit:
#if HDEBUG
    Delay(100L);		/* This is dangerous! */
    uninitsyslog();
#endif				/* HDEBUG */

    if (done & 2)
	UnLoadSeg(DevNode->dn_SegList); /* This is real fun. We are still */
    if (done & (2 | 1))
	DevNode->dn_SegList = 0;	/* Forbid()den, fortunately */

    CloseLibrary((struct Library *)DOSBase);

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
#if HDEBUG
    if (volnode != VolNode) {
	debug(("volnode != VolNode %lx != %lx\n",
	    volnode, VolNode));
    }
    if (volnode->dl_Task != DosPort) {
	debug(("volnode->dl_Task != DosPort %lx != %lx\n",
	    volnode->dl_Task, DosPort));
    }
    if (fl->fl_Task != DosPort) {
	debug(("fl->fl_Task != DosPort %lx != %lx\n",
	    fl->fl_Task, DosPort));
    }
#endif

    if (newlock = dosalloc((ulong)sizeof (*newlock))) {
	newlock->fl_Key = (ulong) msfl;
#if 1
	newlock->fl_Task = DosPort;
#else
	/*
	 * This may help the MultiFileSystem?
	 * Using fl->fl_Task also sounds like a good idea but with MFS
	 * that seems to be a bogus address (0xf80a66 is in ROM!)
	 * But volnode->dl_Task quickly causes deadlocks too.
	 * So don't do this.
	 */
	newlock->fl_Task = volnode->dl_Task;
#endif
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
    struct FileLock *fl;
    struct FileLock **flp;
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

BOOL
AddVolNode(struct DeviceList *volnode)
{
    long	    success = DOSFALSE;

    if (DOSBase->dl_lib.lib_Version >= 37) {
	struct DosList *dl;

	dl = AttemptLockDosList(LDF_VOLUMES | LDF_WRITE);
	if ((ULONG)dl & ~1) {
	    success = AddDosEntry((struct DosList *)volnode);
	    UnLockDosList(LDF_VOLUMES | LDF_WRITE);
	}
    } else {
	struct DosInfo *di;

	di = BTOC(((struct RootNode *) DOSBase->dl_Root)->rn_Info);
	Forbid();
	volnode->dl_Next = di->di_DevInfo;
	di->di_DevInfo = (long) CTOB(volnode);
	Permit();
	success = DOSTRUE;
    }
    return success;
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
    struct DeviceList *volnode;
    char	   *volname;	    /* This is my volume name */

    if (volnode = dosalloc((ulong)sizeof (struct DeviceList))) {
	if (volname = dosalloc(32L)) {
	    volname[0] = strlen(name);
	    strcpy(volname + 1, name);	    /* Make sure \0 terminated */

	    volnode->dl_Type = DLT_VOLUME;
	    volnode->dl_Task = DosPort;
	    volnode->dl_DiskType = IDDiskType;
	    volnode->dl_Name = (BSTR)CTOB(volname);
	    volnode->dl_VolumeDate = *date;
	    volnode->dl_MSFileLockList = (ULONG)NULL;

	    if (AddVolNode(volnode)== DOSFALSE)
		goto error;
	} else {
	error:
	    dosfree((ulong *)volnode);
	    volnode = NULL;
	}
    } else {
	error = ERROR_NO_FREE_STORE;
    }

    debug(("NewVolNode: returns %p\n", volnode));
    return volnode;
}

/*
 *  Get the current VolNode a new name from the volume label.
 */

void
NewVolNodeName()
{
    if (VolNode) {
	char *volname = BTOC(VolNode->dl_Name);

	strncpy(volname + 1, Disk.vollabel.de_Msd.msd_Name, L_8+L_3);
	ZapSpaces(volname + 1, volname + 1 + L_8+L_3);
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
    int deallocate = 0;

    debug(("FreeVolNode %08lx\n", volnode));

    if (volnode == NULL)
	return;

    if (DOSBase->dl_lib.lib_Version >= 37) {
	struct DosList *dl;

	dl = AttemptLockDosList(LDF_VOLUMES | LDF_WRITE);
	/*
	 * Maybe the device list is locked when we are called as a result
	 * of a packet. Don't deadlock on that condition.
	 * Additionally, a hack from the Guru Book p. 393.
	 */
	if ((ULONG)dl & ~1) {
	    (void)RemDosEntry((struct DosList *)volnode);
	    deallocate = 1;
	    UnLockDosList(LDF_VOLUMES | LDF_WRITE);
	} else {
	    /*
	     * Failed. Defer to later. If MustFreeVolNode was already set,
	     * that volume is remembered even though it shouldn't.
	     * I hope this is not very likely.
	     */
	    debug(("MustFreeVolNode %lx := %lx\n", MustFreeVolNode, volnode));
	    MustFreeVolNode = volnode;
	}
    } else {
	struct DosInfo *di = BTOC(((struct RootNode *) DOSBase->dl_Root)->rn_Info);
	struct DeviceList *dl;
	void  *dlp;

	dlp = &di->di_DevInfo;
	Forbid();
	for (dl = BTOC(di->di_DevInfo); dl && dl != volnode; dl = BTOC(dl->dl_Next))
	    dlp = &dl->dl_Next;
	if (dl == volnode) {
	    *(BPTR *) dlp = dl->dl_Next;
	    deallocate = 1;
	}
#if HDEBUG
	else {
	    debug(("****PANIC: Unable to find volume node\n"));
	}
#endif				/* HDEBUG */
	Permit();
    }

    if (deallocate) {
	dosfree(BTOC(volnode->dl_Name));
	dosfree((ulong *)volnode);
    }

    if (volnode == VolNode)
	VolNode = NULL;
}

void
FreeVolNodeDeferred(void)
{
    struct DeviceList *tmp = MustFreeVolNode;
    debug(("FreeVolNodeDeferred\n"));
    MustFreeVolNode = NULL;
    FreeVolNode(tmp);
}

/*
 *  This is also called from the real handler when the last lock on a
 *  volume is UnLock()ed, or the last file has been Close()d.
 */

int
MayFreeVolNode(volnode)
struct DeviceList *volnode;
{
    debug(("MayFreeVolNode volnode %p, dl_LockList %p\n", volnode, volnode->dl_LockList));
    if (volnode->dl_LockList == NULL) {
	FreeVolNode(volnode);
	return TRUE;
    }

    return FALSE;
}

#if defined(PROMISE_NOT_TO_DIE)
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
	     * Alternatively, in DiskRemoved() the OpenCount could
	     * be decremented for each lock (and dl_Task cleared?).
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

long
DiskRemoved()
{
    debug(("DiskRemoved %08lx\n", VolNode));

#if INPUTDEV
    if (IDDiskType != ID_NO_DISK_PRESENT)
	InputDiskRemoved();
#endif

    if (VolNode == NULL) {
	IDDiskType = ID_NO_DISK_PRESENT;/* really business of MSDiskRemoved */
	return DOSTRUE;
    }

    VolNode->dl_Task = NULL;
    MSDiskRemoved((struct LockList **)&VolNode->dl_MSFileLockList);
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
#if defined(PROMISE_NOT_TO_DIE)
	RedirectLocks(volnode);
#endif
	MSDiskInserted((struct LockList **)&volnode->dl_MSFileLockList, volnode);
	volnode->dl_MSFileLockList = (BPTR)NULL;
#if INPUTDEV
	if (IDDiskType != ID_NO_DISK_PRESENT)
	    InputDiskInserted();
#endif
    }
}

/*
 * Test if we have a DD or HD floppy right now.
 * If the TD_GETDRIVETYPE fails or produces weird values, do nothing.
 * Otherwise, select the correct sectors/track value, originally
 * derived from the mountlist.
 */
void
CheckDriveType(void)
{
    struct IOExtTD *req = DiskIOReq;

    debug(("CheckDriveType()\n"));
    req->iotd_Req.io_Command = TD_GETDRIVETYPE;
    /*
     * If this fails or gives weird values, it probably is not
     * a floppy drive, and we don't mess with the defaults.
     */
    if (MyDoIO(&req->iotd_Req) == 0) {
	int		spt;

	switch (req->iotd_Req.io_Actual) {
	case DRIVE3_5:
	case DRIVE5_25:
	    debug(("DD floppy\n"));
	    spt = Partition.spt_dd;
	    break;
	case DRIVE3_5_150RPM:
	    debug(("HD floppy\n"));
	    spt = Partition.spt_hd;
	    break;
	default:
	    debug(("strange drive type: %d\n", req->iotd_Req.io_Actual));
	    goto no_floppy;
	}
	DefaultDisk.spt = spt;
	if (Environ)
	    Environ->de_BlocksPerTrack = spt;
	DefaultDisk.nsects = (Environ->de_HighCyl - Environ->de_LowCyl + 1) *
			     DefaultDisk.nsides * spt;

	/*
	 * Suggest a minimum value for the number of FAT sectors.
	 * Here we assume all sectors are to be used for clusters, which is not
	 * really true, but simpler, and gives a conservative value. Besides,
	 * the number of available sectors also depends on the FAT size, so
	 * the whole calculation (if done correctly) would be recursive. In
	 * practice, it may occasionally suggest one sector too much.
	 */
	{
	    long	    nclusters;
	    long	    nbytes;

	    nclusters = MS_FIRSTCLUST + (DefaultDisk.nsects+DefaultDisk.spc-1)/
					 DefaultDisk.spc;
	    if (nclusters > 0xFF7) /* 16-bit FAT entries */
		nbytes = nclusters * 2;
	    else		  /* 12-bit FAT entries */
		nbytes = (nclusters * 3 + 1) / 2;
	    DefaultDisk.spf = (nbytes + DefaultDisk.bps - 1) / DefaultDisk.bps;
	    /* Hack for floppies */
	    if (DefaultDisk.spf < MS_SPF)
		DefaultDisk.spf = MS_SPF;
	}
    }
no_floppy:;
}

struct DeviceList *
WhichDiskInserted()
{
    char name[34];
    struct DateStamp date;
    struct DeviceList *dl = NULL;

    CheckDriveType();

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

    debug(("WhichDiskInserted() done\n"));

    return dl;
}

void
DiskChange(void)
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
