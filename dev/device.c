/*-
 * $Id: device.c,v 1.3 89/12/17 21:29:37 Rhialto Exp Locker: Rhialto $
 * $Log:	device.c,v $
 * Revision 1.3  89/12/17  21:29:37  Rhialto
 * Revision 1.1  89/12/17  20:03:55  Rhialto
 *
 * DEVICE.C
 *
 * The messydisk.device code that makes it a real Exec .device.
 * Mostly based on the 1.1 RKM example and Matt Dillon's library code.
 *
 * This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
-*/

#include "dev.h"
#include "device.h"

#undef DEBUG			/**/
#ifdef DEBUG
#   define	debug(x)  dbprintf x
#else
#   define	debug(x)
#endif

/*
 * The first executable location. This should return an error in case
 * someone tried to run you as a program (instead of loading you as a
 * device)
 */
/* INDENT OFF */
#asm
	moveq.l #20,d0
	rts
#endasm
/* INDENT ON */

/*
 * A romtag structure. Both "exec" and "ramlib" look for this structure to
 * discover magic constants about you (such as where to start running you
 * from...).
 */
/* INDENT OFF */
#asm
	public	__H0_end
_EndCode equ	__H0_end
	public	_RomTag
_RomTag:
	dc.w	$4AFC		; RTC_MATCHWORD
	dc.l	_RomTag 	; rt_MatchTag
	dc.l	__H0_end	; rt_EndSkip
	dc.b	0		; rt_Flags (no RTF_AUTOINIT)
	dc.b	VERSION 	; rt_Version
	dc.b	3		; rt_Type  NT_DEVICE
	dc.b	RTPRI		; rt_Pri
	dc.l	_DevName	; rt_Name
	dc.l	_idString	; rt_IdString
	dc.l	_Init		; rt_Init
#endasm
/* INDENT ON */

char		DevName[] = "messydisk.device";
char		idString[] = "messydisk.device $Revision: 1.3 $ $Date: 89/12/17 21:29:37 $\r\n";

/*
 * -30-6*X  Library vectors:
 */

void		(*LibVectors[]) () =
{
    _DevOpen, _DevClose, _DevExpunge, _LibNull,

    _DevBeginIO,
    _DevAbortIO,
    (void (*) ()) -1
};

/*
 * Device commands:
 */

void		(*funcTable[]) () = {
    CMD_Invalid, CMD_Reset, CMD_Read, CMD_Write, CMD_Update, CMD_Clear,
    CMD_Stop, CMD_Start, CMD_Flush, TD_Motor, TD_Seek, TD_Format,
    TD_Remove, TD_Changenum, TD_Changestate, TD_Protstatus, TD_Rawread,
    TD_Rawwrite, TD_Getdrivetype, TD_Getnumtracks, TD_Addchangeint,
    TD_Remchangeint,
};

/*
 * Here begin the system interface commands. When the user calls
 * OpenDevice/CloseDevice/RemDevice, this eventually gets trahslated into
 * a call to the following routines (Open/Close/Expunge).  Exec has
 * already put our device pointer in A6 for us.  Exec has turned off task
 * switching while in these routines (via Forbid/Permit), so we should not
 * take too long in them.
 */
/* INDENT OFF */
#asm
	public	_Init
_Init:				;a0=segment list
	movem.l D2-D3/A0/A6,-(sp)
	jsr	_CInit
	movem.l (sp)+,D2-D3/A0/A6
	rts

	public	__DevOpen
__DevOpen:			;d0=unitnum,d1=flags,a1=ioreq,a6=device
	movem.l D0-D3/A1/A6,-(sp)
	jsr	_DevOpen
	movem.l (sp)+,D0-D3/A1/A6
	rts

	public	__DevClose
__DevClose:			;a1=ioreq,a6=device
	movem.l D2-D3/A1/A6,-(sp)
	jsr	_DevClose
	movem.l (sp)+,D2-D3/A1/A6
	rts

	public	__DevExpunge
__DevExpunge:			;a6=device
	movem.l D2-D3/A6,-(sp)
	jsr	_DevExpunge
	movem.l (sp)+,D2-D3/A6
	rts

	public	__LibNull
__LibNull:
	clr.l	d0
	rts

	public	__DevBeginIO
__DevBeginIO:			;a1=ioreq,a6=device
	movem.l D2-D3/A1/A6,-(sp)
	jsr	_DevBeginIO
	movem.l (sp)+,D2-D3/A1/A6
	rts

	public	__DevAbortIO
__DevAbortIO:			;a1=ioreq,a6=device
	movem.l D2-D3/A1/A6,-(sp)
	jsr	_DevAbortIO
	movem.l (sp)+,D2-D3/A1/A6
	rts
#endasm

#ifdef HANDLE_IO_QUICK
#asm
;;;;
;
;   C interface to the atomic set bit and test old value instruction.
;
;   Called as	BSET_ACTIVE(byte *address).
;
;   Old value of the bit returned all over d0.w

_BSET_ACTIVE:
	move.l	4(sp),a0
	bset	#0,(a0)         ; UNITB_ACTIVE
	sne	d0
	rts

#endasm
#endif
/* INDENT ON */

long		SysBase;	/* Argh! A global variable! */

/*
 * The Initialization routine is given only a seglist pointer.	Since we
 * are NOT AUTOINIT we must construct and add the device ourselves and
 * return either NULL or the device pointer.  Exec has Forbid() for us
 * during the call.
 *
 * If you have an extended device structure you must specify the size of the
 * extended structure in MakeLibrary().
 */

DEV	       *
CInit(D2, D3, segment)
ulong		D2,
		D3;
long		segment;
{
    DEV 	   *dev;

    SysBase = *(long *) 4;
#ifdef DEBUG
    dbinit();
#endif
    dev = MakeLibrary(LibVectors, NULL, NULL, (long) sizeof (DEV), NULL);
    if (DevInit(dev)) {
	dev->dev_Node.ln_Type = NT_DEVICE;
	dev->dev_Node.ln_Name = DevName;
	dev->dev_Flags = LIBF_CHANGED | LIBF_SUMUSED;
	dev->dev_Version = VERSION;
	dev->dev_Revision = REVISION;
	dev->dev_IdString = (APTR) idString;
	dev->md_Seglist = segment;
	AddDevice(dev);
	return (dev);
    }
    FreeMem((char *) dev - dev->dev_NegSize, dev->dev_NegSize + dev->dev_PosSize);
    return NULL;
}

/*
 * Open is given the device pointer, unitno and flags.	Either return the
 * device pointer or NULL.  Remove the DELAYED-EXPUNGE flag. Exec has
 * Forbid() for us during the call.
 */

void
DevOpen(unitno, flags, D2, D3, ioreq, dev)
ulong		unitno;
ulong		flags;
ulong		D2,
		D3;
struct IOStdReq *ioreq;
DEV	       *dev;
{
    UNIT	   *unit;

    debug(("OpenDevice unit %ld, flags %lx\n", unitno, flags));
    if (unitno >= MD_NUMUNITS)
	goto error;

    if ((unit = dev->md_Unit[unitno]) == NULL) {
	if ((unit = UnitInit(dev, unitno)) == NULL)
	    goto error;
	dev->md_Unit[unitno] = unit;
    }
    ioreq->io_Unit = (struct Unit *) unit;

    ++unit->mu_OpenCnt;
    ++dev->dev_OpenCnt;
    dev->dev_Flags &= ~LIBF_DELEXP;

    return;

error:
    ioreq->io_Error = IOERR_OPENFAIL;
}

/*
 * Close is given the device pointer and the io request.  Be sure not to
 * decrement the open count if already zero.	If the open count is or
 * becomes zero AND there is a LIBF_DELEXP, we expunge the device and
 * return the seglist.	Otherwise we return NULL.
 *
 * Note that this routine never sets LIBF_DELEXP on its own.
 *
 * Exec has Forbid() for us during the call.
 */

long
DevClose(D2, D3, ioreq, dev)
ulong		D2,
		D3;
struct IOStdReq *ioreq;
DEV	       *dev;
{
    UNIT	   *unit;

    unit = (UNIT *) ioreq->io_Unit;
    debug(("CloseDevice io %08lx unit %08lx\n", ioreq, unit));

    /*
     * See if the unit is still in use. If not, close it down. This may
     * need to do an update, which requires the ioreq.
     */

    if (unit->mu_OpenCnt && --unit->mu_OpenCnt == 0) {
	dev->md_Unit[unit->mu_UnitNr] = NULL;
	UnitCloseDown(ioreq, dev, unit);
    }
    /*
     * Make sure the ioreq is not used again.
     */
    ioreq->io_Unit = (void *) -1;
    ioreq->io_Device = (void *) -1;

    if (dev->dev_OpenCnt && --dev->dev_OpenCnt)
	return (NULL);
    if (dev->dev_Flags & LIBF_DELEXP)
	return (DevExpunge(D2, D3, dev));
    return (NULL);
}

/*
 * We expunge the device and return the Seglist ONLY if the open count is
 * zero. If the open count is not zero we set the DELAYED-EXPUNGE
 * flag and return NULL.
 *
 * Exec has Forbid() for us during the call.  NOTE ALSO that Expunge might be
 * called from the memory allocator and thus we CANNOT DO A Wait() or
 * otherwise take a long time to complete (straight from RKM).
 *
 * Apparently RemLibrary(lib) calls our expunge routine and would therefore
 * freeze if we called it ourselves.  As far as I can tell from RKM,
 * DevExpunge(lib) must remove the device itself as shown below.
 */

long
DevExpunge(D2, D3, dev)
ulong		D2,
		D3;
DEV	       *dev;
{
    long	    Seglist;

    if (dev->dev_OpenCnt) {
	dev->dev_Flags |= LIBF_DELEXP;
	return (NULL);
    }
    Remove(dev);
    DevCloseDown(dev);          /* Should be quick! */
#ifdef DEBUG
    dbuninit();
#endif
    Seglist = dev->md_Seglist;
    FreeMem((char *) dev - dev->dev_NegSize,
	    (long) dev->dev_NegSize + dev->dev_PosSize);
    return (Seglist);
}

/*
 * BeginIO entry point. We don't handle any QUICK requests, we just send
 * the request to the proper unit to handle.
 */

void
DevBeginIO(D2, D3, ioreq, dev)
ulong		D2,
		D3;
register struct IOStdReq *ioreq;
DEV	       *dev;
{
    UNIT	   *unit;

    /*
     * Bookkeeping.
     */
    unit = (UNIT *) ioreq->io_Unit;
    debug(("BeginIO: io %08lx dev %08lx u %08lx\n", ioreq, dev, unit));

    /*
     * See if the io command is within range.
     */
    if (STRIP(ioreq->io_Command) > TD_LASTCOMM)
	goto NoCmd;

#ifdef HANDLE_IO_QUICK
    Forbid();                   /* Disable(); is a bit too strong for us. */
#endif

    /*
     * Process all immediate commands no matter what. Don't even require
     * an exclusive lock on the unit.
     */
    if (IMMEDIATE & (1L << STRIP(ioreq->io_Command)))
	goto Immediate;

    /*
     * We don't handle any QUICK I/O since that only gives trouble with
     * message ports and so. Other devices normally would include the code
     * below.
     */
#ifdef HANDLE_IO_QUICK
    /*
     * See if the user does not request QUICK IO. If not, it is likely to
     * be async and therefore we don't do it sync.
     */
    if (!(ioreq->io_Flags & IOF_QUICK))
	goto NoQuickRequested;

    /*
     * See if the unit is STOPPED. If so, queue the msg.
     */
    if (unit->mu_Flags & UNITF_STOPPED)
	goto QueueMsg;

    /*
     * This is not an immediate command. See if the device is busy. If
     * not, process the action in this (the caller's) context.
     */
    if (!BSET_ACTIVE(&unit->mu_Flags))
	goto Immediate;
#endif

    /*
     * We need to queue the device. Clear the QUICK flag.
     */
QueueMsg:
    ioreq->io_Flags &= ~IOF_QUICK;
NoQuickRequested:
#ifdef HANDLE_IO_QUICK
    Permit();                   /* Enable(); is a bit too strong for us. */
#endif
    PutMsg(&unit->mu_Port, ioreq);

    return;

Immediate:
#ifdef HANDLE_IO_QUICK
    Permit();                   /* Enable(); is a bit too strong for us. */
#endif
    debug(("BeginIO: Immediate\n"));
    ioreq->io_Error = TDERR_NoError;
    PerformIO(ioreq, unit);
    return;

NoCmd:
    ioreq->io_Error = IOERR_NOCMD;
    TermIO(ioreq);
    return;

}

/*
 * Terminate an io request. Called (normally) for every BeginIO. 'Funny'
 * commands that don't call TermIO, or call it multiple times, may not be
 * properly handled unless you are careful. TD_ADDCHANGEINT and
 * TD_REMCHANGEINT are obvious examples.
 */

void
TermIO(ioreq)
register struct IOStdReq *ioreq;
{
    register UNIT  *unit;

    unit = (UNIT *) ioreq->io_Unit;
    debug(("TermIO: io %08lx u %08lx %ld %d\n", ioreq, unit,
	   ioreq->io_Actual, ioreq->io_Error));

#ifdef HANDLE_IO_QUICK
    /*
     * Since immediate commands don't even require an exclusive lock on
     * the unit, don't unlock it.
     */
    if (IMMEDIATE & (1L << STRIP(ioreq->io_Command)))
	goto Immediate;

    /*
     * We may need to turn the active (lock) bit off, but not if we are
     * within the task.
     */
    if (unit->mu_Flags & UNITF_INTASK)
	goto Immediate;

    unit->mu_Flags &= ~UNITF_ACTIVE;

    /*
     * The task may have work to do that came in while we were processing
     * in the caller's context.
     */
    if (unit->mu_Flags & UNITF_WAKETASK) {
	unit->mu_Flags &= ~UNITF_WAKETASK;
	WakePort(&unit->mu_Port);
    }
#endif

Immediate:
    /*
     * If the quick bit is still set then wen don't need to reply the msg
     * -- just return to the user.
     */

    if (!(ioreq->io_Flags & IOF_QUICK))
	ReplyMsg(&ioreq->io_Message);

    return;
}

/*
 * AbortIO entry point. We don't abort IO here.
 */

long
DevAbortIO(D2, D3, ioreq, dev)
ulong		D2,
		D3;
DEV	       *dev;
struct IOStdReq *ioreq;
{
    return 1;
}

void
WakePort(port)
register struct MsgPort *port;
{
    Signal(port->mp_SigTask, 1L << port->mp_SigBit);
}

/*
 * This is the main loop of the Unit tasks. It must be very careful with
 * global data.
 */

void
UnitTask()
{
    /* DEV *dev; */
    UNIT	   *unit;
    long	    waitmask;
    struct IOExtTD *ioreq;

    {
	struct Task    *task,
		       *FindTask();

	task = FindTask(NULL);
	unit = (UNIT *) task->tc_UserData;
	/* dev = unit->mu_Dev; */
	task->tc_UserData = NULL;
    }

    /*
     * Now finish initializing the message ports and other signal things
     */

    {
	byte		sigbit;

	unit->DiskReplyPort.mp_SigBit = AllocSignal(-1L);
	unit->DiskReplyPort.mp_Flags = PA_SIGNAL;

	sigbit = AllocSignal(-1L);
	unit->mu_Port.mp_SigBit = sigbit;
	unit->mu_Port.mp_Flags = PA_SIGNAL;
	waitmask = 1L << sigbit;

	unit->mu_DmaSignal = AllocSignal(-1L);
    }

    for (;;) {
	debug(("Task: Waiting... "));
	Wait(waitmask);

	/*
	 * See if we are stopped.
	 */
	if (unit->mu_Flags & UNITF_STOPPED)
	    continue;

#ifdef HANDLE_IO_QUICK
	/*
	 * Lock the device. If it fails, we have set a flag such that the
	 * TermIO wakes us again.
	 */
	unit->mu_Flags |= UNITF_WAKETASK;
	if (BSET_ACTIVE(&unit->mu_Flags))
	    continue;

	unit->mu_Flags |= UNITF_INTASK;
#endif

	while (ioreq = (struct IOExtTD *) GetMsg(&unit->mu_Port)) {
	    debug(("Task: io %08lx %x\n", ioreq, ioreq->iotd_Req.io_Command));
	    ioreq->iotd_Req.io_Error = 0;
	    PerformIO((&ioreq->iotd_Req), unit);
	}

#ifdef HANDLE_IO_QUICK
	unit->mu_Flags &= ~(UNITF_ACTIVE | UNITF_INTASK | UNITF_WAKETASK);
#endif
    }
}

void
CMD_Invalid(ioreq, unit)
struct IOStdReq *ioreq;
UNIT	       *unit;
{
    ioreq->io_Error = IOERR_NOCMD;
    TermIO(ioreq);
}

void
CMD_Stop(ioreq, unit)
struct IOExtTD *ioreq;
UNIT	       *unit;
{
    unit->mu_Flags |= UNITF_STOPPED;
    TermIO(ioreq);
}

void
CMD_Start(ioreq, unit)
struct IOExtTD *ioreq;
UNIT	       *unit;
{
    unit->mu_Flags &= ~UNITF_STOPPED;
    WakePort(&unit->mu_Port);
    TermIO(ioreq);
}

void
CMD_Flush(ioreq, unit)
struct IOExtTD *ioreq;
UNIT	       *unit;
{
    register struct IOStdReq *req;

    /* Flush our own command queue */
    Forbid();
    while (req = (struct IOStdReq *) GetMsg(unit->mu_Port)) {
	req->io_Error = IOERR_ABORTED;
	ReplyMsg(&req->io_Message);
    }
    Permit();

    WakePort(&unit->mu_Port);
    TermIO(ioreq);
}

void
TrackdiskGateway(ioreq, unit)
register struct IOExtTD *ioreq;
UNIT	       *unit;
{
    register struct IOExtTD *tdioreq;

    debug(("Trackdisk: %x ", ioreq->iotd_Req.io_Command));
    tdioreq = unit->DiskIOReq;

    /*
     * Clone almost the entire io request to relay to the
     * trackdisk.device.
     */

    tdioreq->iotd_Req.io_Command = ioreq->iotd_Req.io_Command;
    tdioreq->iotd_Req.io_Flags = ioreq->iotd_Req.io_Flags | IOF_QUICK;
    tdioreq->iotd_Req.io_Length = ioreq->iotd_Req.io_Length;
    tdioreq->iotd_Req.io_Data = ioreq->iotd_Req.io_Data;
    tdioreq->iotd_Req.io_Offset = ioreq->iotd_Req.io_Offset;
    if (ioreq->iotd_Req.io_Command & TDF_EXTCOM) {
	tdioreq->iotd_Count = ioreq->iotd_Count;
	tdioreq->iotd_SecLabel = ioreq->iotd_SecLabel;
    }
    BeginIO(tdioreq);
    WaitIO(tdioreq);

    ioreq->iotd_Req.io_Error = tdioreq->iotd_Req.io_Error;
    ioreq->iotd_Req.io_Actual = tdioreq->iotd_Req.io_Actual;

    TermIO(ioreq);
}

#ifdef DEBUG
/* DEBUGGING			    */
struct MsgPort *Dbport; 	/* owned by the debug process	    */
struct MsgPort *Dback;		/* owned by the DOS device driver  */
short		DBEnable;
struct SignalSemaphore PortUse;

#define CTOB(x) (void *)(((long)(x))>>2)        /* BCPL conversion */

/*
 * DEBUGGING CODE.	You cannot make DOS library calls that access
 * other devices from within a device driver because the caller may not be
 * a process.  If you need to make such calls you must create a port and
 * construct the DOS messages yourself.  I do not do this.  To get
 * debugging info out another PROCESS is created to which debugging
 * messages can be sent. The replyport gets a new SigTask for every
 * dbprintf call, therefore the semaphore.
 */

extern void	debugproc();
struct Library *DOSBase,
	       *OpenLibrary();

dbinit()
{
    struct Task    *task = FindTask(NULL);

    DOSBase = OpenLibrary("dos.library", 0L);
    Dback = CreatePort("Dback", -1L);
    FreeSignal((long) Dback->mp_SigBit);
    Dback->mp_SigBit = 2;
    InitSemaphore(&PortUse);
    CreateProc("messydisk_DB", (long) TASKPRI + 1, CTOB(debugproc), 2000L);
    WaitPort(Dback);            /* handshake startup    */
    GetMsg(Dback);              /* remove dummy msg     */
    DBEnable = 1;
    dbprintf("Debugger running V1.11\n");
}

dbuninit()
{
    struct Message  killmsg;

    if (Dbport) {
	killmsg.mn_Length = 0;	/* 0 means die	    */
	ObtainSemaphore(&PortUse);
	Dback->mp_SigTask = FindTask(NULL);
	PutMsg(Dbport, &killmsg);
	WaitPort(Dback);        /* He's dead jim!      */
	GetMsg(Dback);
	ReleaseSemaphore(&PortUse);
	Dback->mp_SigBit = -1;
	DeletePort(Dback);

	/*
	 * Since the debug process is running at a greater priority, I am
	 * pretty sure that it is guarenteed to be completely removed
	 * before this task gets control again.
	 */
    }
    CloseLibrary(DOSBase);
}

dbprintf(a, b, c, d, e, f, g, h, i, j)
long		a, b, c, d, e, f, g, h, i, j;
{
    struct {
	struct Message	msg;
	char		buf[256];
    }		    msgbuf;
    register struct Message *msg = &msgbuf.msg;
    register long   len;

    if (Dbport && DBEnable) {
	ObtainSemaphore(&PortUse);      /* sprintf is not re-entrant */
	sprintf(msgbuf.buf, a, b, c, d, e, f, g, h, i, j);
	len = strlen(msgbuf.buf) + 1;
	msg->mn_Length = len;	/* Length NEVER 0	 */
	Dback->mp_SigTask = FindTask(NULL);
	PutMsg(Dbport, msg);
	WaitPort(Dback);
	GetMsg(Dback);
	ReleaseSemaphore(&PortUse);
    }
}

/*
 * BTW, the DOS library used by debugmain() was actually opened by the
 * opener of the device driver.
 */

debugmain()
{
    register struct Message *msg;
    register long   len;
    register void  *fh,
		   *Open();
    void	   *fh2;
    struct Message  DummyMsg;

    Dbport = CreatePort("Dbport", -1L);
    fh = Open("CON:0/20/640/101/Device debug", MODE_NEWFILE);
    fh2 = Open("PAR:", MODE_OLDFILE);
    PutMsg(Dback, &DummyMsg);
    for (;;) {
	WaitPort(Dbport);
	msg = GetMsg(Dbport);
	len = msg->mn_Length;
	if (len == 0)
	    break;
	--len;			/* Fix length up	 */
	if (DBEnable & 1)
	    Write(fh, msg + 1, len);
	if (DBEnable & 2)
	    Write(fh2, msg + 1, len);
	PutMsg(Dback, msg);
    }
    Close(fh);
    Close(fh2);
    DeletePort(Dbport);
    PutMsg(Dback, msg);         /* Kill handshake  */
}

/*
 * The assembly tag for the DOS process:  CNOP causes alignment problems
 * with the Aztec assembler for some reason.  I assume then, that the
 * alignment is unknown.  Since the BCPL conversion basically zero's the
 * lower two bits of the address the actual code may start anywhere within
 * 8 bytes of address (remember the first longword is a segment pointer
 * and skipped).  Sigh....  (see CreateProc() above).
 */
/* INDENT OFF */
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

#endif				/* DEBUG */
