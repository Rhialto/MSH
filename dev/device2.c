/*-
 * $Id: device2.c,v 1.54 1993/06/24 04:56:00 Rhialto Exp $
 * $Log: device2.c,v $
 * Revision 1.54  1993/06/24  04:56:00	Rhialto
 * Switch to RTF_AUTOINIT, saves a few bytes. DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:10:25  Rhialto
 * Add TD_Getgeometry, TD_Eject.
 *
 * Revision 1.52  92/09/06  00:04:07  Rhialto
 * Include $VER in version string.
 *
 * Revision 1.51  92/04/17  15:41:55  Rhialto
 * Freeze for MAXON3. Change cyl+side units to track units.
 *
 * Revision 1.46  91/10/06  18:22:08  Rhialto
 * Freeze for MAXON; new syslog stuff
 *
 * Revision 1.42  91/06/13  23:45:09  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:55:48  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.32  90/11/23  23:54:36  Rhialto
 * Prepare for syslog
 *
 * Revision 1.30  90/06/04  23:18:39  Rhialto
 * Release 1 Patch 3
 *
 * DEVICE.C
 *
 * The messydisk.device code that makes it a real Exec .device.
 * Mostly based on the 1.1 RKM example and Matt Dillon's library code.
 *
 * This code is (C) Copyright 1989-1993 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include <exec/initializers.h>
#include "device.h"

/*#undef DEBUG			*/
#ifdef DEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif
/* INDENT ON */

Prototype __geta4 DEV *Init(__A0 long segment, __D0 struct MessyDevice *dev, __A6 struct ExecBase *execbase);
Prototype __geta4 void DevOpen(__D0 ulong unitno, __D1 ulong flags, __A1 struct IOStdReq *ioreq, __A6 DEV *dev);
Prototype __geta4 long DevClose(__A1 struct IOStdReq *ioreq, __A6 DEV *dev);
Prototype __geta4 long DevExpunge(__A6 DEV *dev);
Prototype __geta4 void DevBeginIO(__A1 struct IOStdReq *ioreq, __A6 DEV *dev);
Prototype __geta4 long DevAbortIO(__A1 struct IOStdReq *ioreq, __A6 DEV *dev);

Prototype void TermIO(struct IOStdReq *ioreq);
Prototype void WakePort(struct MsgPort *port);
Prototype __geta4 void UnitTask(void);
Prototype void CMD_Invalid(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Stop(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Start(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Flush(struct IOStdReq *ioreq, UNIT *unit);
Prototype void TrackdiskGateway(struct IOStdReq *ioreq, UNIT *unit);

Prototype const char DevName[];
Prototype const char idString[];

const char	DevName[] = "messydisk.device";
const char	idString[] = "$VER: messydisk.device $Revision: 1.54 $ $Date: 1993/06/24 04:56:00 $\r\n";

/*
 * Device commands:
 */

const void	(*funcTable[]) (struct IOStdReq *, UNIT *) = {
    CMD_Invalid, CMD_Reset, CMD_Read, CMD_Write, CMD_Update, CMD_Clear,
    CMD_Stop, CMD_Start, CMD_Flush, TD_Motor, TD_Seek, TD_Format,
    TD_Remove, TD_Changenum, TD_Changestate, TD_Protstatus, TD_Rawread,
    TD_Rawwrite, TD_Getdrivetype, TD_Getnumtracks, TD_Addchangeint,
    TD_Remchangeint, TD_Getgeometry, TD_Eject,
};

#define LAST_TD_COMM	    TD_EJECT

long		SysBase;	/* Argh! A global variable! */

/*
 * The Initialization routine is given a seglist pointer, the device
 * base pointer, and the Exec base pointer. We are being called from
 * InitResident. Exec has Forbid() for us during the call.
 */


__geta4 DEV    *
Init(segment, dev, execbase)
__A0 long	segment;
__D0 DEV       *dev;
__A6 struct ExecBase *execbase;
{
    SysBase = *(long *) 4;
#ifdef DEBUG
    initsyslog();
    debug(("seg %lx, dev %lx, sys %lx\n", segment, dev, execbase));
#endif
    if (DevInit(dev)) {
	dev->md_Seglist = segment;
	debug(("init done.\n"));
	return dev;
    }
    FreeMem((char *) dev - dev->dev_NegSize, dev->dev_NegSize + dev->dev_PosSize);
    return NULL;
}

/*
 * Open is given the device pointer, unitno and flags.	Either return the
 * device pointer or NULL.  Remove the DELAYED-EXPUNGE flag. Exec has
 * Forbid() for us during the call.
 */

__geta4 void
DevOpen(unitno, flags, ioreq, dev)
__D0 ulong	unitno;
__D1 ulong	flags;
__A1 struct IOStdReq *ioreq;
__A6 DEV       *dev;
{
    UNIT	   *unit;
    int 	    nrflags;

    debug(("OpenDevice unit %ld, flags %lx\n", unitno, flags));
    ++dev->dev_OpenCnt;

    if (nrflags = unitno / MD_NUMUNITS) {
	/* Units 4..7 are fixed 40-tracks */
	/* Units 8..11 are fixed non-40-tracks */
	switch (nrflags) {
	case 1:
	    flags |= IOMDF_40TRACKS | IOMDF_FIXFLAGS;
	    unitno -= MD_NUMUNITS;
	    break;
	case 2:
	    flags &= ~IOMDF_40TRACKS;
	    flags |= IOMDF_FIXFLAGS;
	    unitno -= 2 * MD_NUMUNITS;
	    break;
	}
    }
    if (unitno >= MD_NUMUNITS)
	goto error;

    if ((unit = dev->md_Unit[unitno]) == NULL) {
	if ((unit = UnitInit(dev, unitno, flags)) == NULL)
	    goto error;
	dev->md_Unit[unitno] = unit;
    }
    ioreq->io_Unit = (struct Unit *) unit;

    ++unit->mu_OpenCnt;
    dev->dev_Flags &= ~LIBF_DELEXP;
    ioreq->io_Error = 0;
    ioreq->io_Flags = flags;

    return;

error:
    --dev->dev_OpenCnt;
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

__geta4 long
DevClose(ioreq, dev)
__A1 struct IOStdReq *ioreq;
__A6 DEV       *dev;
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
	UnitCloseDown(dev, unit);
    }
    /*
     * Make sure the ioreq is not used again.
     */
    ioreq->io_Unit = (void *) -1;
    ioreq->io_Device = (void *) -1;

    if (dev->dev_OpenCnt && --dev->dev_OpenCnt)
	return NULL;
    if (dev->dev_Flags & LIBF_DELEXP)
	return DevExpunge(dev);
    return NULL;
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

__geta4 long
DevExpunge(dev)
__A6 DEV       *dev;
{
    long	    Seglist;

    if (dev->dev_OpenCnt) {
	dev->dev_Flags |= LIBF_DELEXP;
	return NULL;
    }
    Remove(&dev->dev_Node);
    DevCloseDown(dev);          /* Should be quick! */
#ifdef DEBUG
    uninitsyslog();
#endif
    Seglist = dev->md_Seglist;
    FreeMem((char *) dev - dev->dev_NegSize,
	    (long) dev->dev_NegSize + dev->dev_PosSize);
    return Seglist;
}

/*
 * BeginIO entry point. We don't handle any QUICK requests, we just send
 * the request to the proper unit to handle.
 */

__geta4 void
DevBeginIO(ioreq, dev)
__A1 struct IOStdReq *ioreq;
__A6 DEV       *dev;
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
    if (STRIP(ioreq->io_Command) > LAST_TD_COMM)
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
    PutMsg(&unit->mu_Port, &ioreq->io_Message);

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
    debug(("TermIO: io %08lx u %08lx %ld %ld\n", ioreq, unit,
	   ioreq->io_Actual, (long)ioreq->io_Error));

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
 * AbortIO entry point. We try to abort IO here.
 */

__geta4 long
DevAbortIO(ioreq, dev)
__A1 struct IOStdReq *ioreq;
__A6 DEV       *dev;
{
    Forbid();
    if (ioreq->io_Flags & IOF_QUICK ||
	IMMEDIATE & (1L << STRIP(ioreq->io_Command))) {
	Permit();
	return 1;
    } else {
	Remove(&ioreq->io_Message.mn_Node);
	Permit();
	ioreq->io_Error = IOERR_ABORTED;
	ReplyMsg(&ioreq->io_Message);

	return 0;
    }
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

__geta4 void
UnitTask()
{
    /* DEV *dev; */
    UNIT	   *unit;
    long	    waitmask;
    struct IOExtTD *ioreq;

    {
	struct Task    *task;

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

	unit->mu_DiskReplyPort.mp_SigBit = AllocSignal(-1L);
	unit->mu_DiskReplyPort.mp_Flags = PA_SIGNAL;

	sigbit = AllocSignal(-1L);
	unit->mu_Port.mp_SigBit = sigbit;
	unit->mu_Port.mp_Flags = PA_SIGNAL;
	waitmask = 1L << sigbit;

	unit->mu_DmaSignal = AllocSignal(-1L);
    }

    for (;;) {
	debug(("Task: Waiting...\n"));
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
	    debug(("Task: io %08lx %lx\n", ioreq, (long)ioreq->iotd_Req.io_Command));
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
struct IOStdReq *ioreq;
UNIT	       *unit;
{
    unit->mu_Flags |= UNITF_STOPPED;
    TermIO(ioreq);
}

void
CMD_Start(ioreq, unit)
struct IOStdReq *ioreq;
UNIT	       *unit;
{
    unit->mu_Flags &= ~UNITF_STOPPED;
    WakePort(&unit->mu_Port);
    TermIO(ioreq);
}

void
CMD_Flush(ioreq, unit)
struct IOStdReq *ioreq;
UNIT	       *unit;
{
    register struct IOStdReq *req;

    /* Flush our own command queue */
    Forbid();
    while (req = (struct IOStdReq *) GetMsg(&unit->mu_Port)) {
	req->io_Error = IOERR_ABORTED;
	ReplyMsg(&req->io_Message);
    }
    Permit();

    WakePort(&unit->mu_Port);
    TermIO(ioreq);
}

void
TrackdiskGateway(ioreq, unit)
register struct IOStdReq *ioreq;
UNIT	       *unit;
{
    register struct IOExtTD *tdioreq;

    debug(("Trackdisk: %lx ", (long)ioreq->io_Command));
    tdioreq = unit->mu_DiskIOReq;

    /*
     * Clone almost the entire io request to relay to the
     * trackdisk.device.
     */

    tdioreq->iotd_Req.io_Command = ioreq->io_Command;
    tdioreq->iotd_Req.io_Flags = ioreq->io_Flags | IOF_QUICK;
    tdioreq->iotd_Req.io_Length = ioreq->io_Length;
    tdioreq->iotd_Req.io_Data = ioreq->io_Data;
    tdioreq->iotd_Req.io_Offset = ioreq->io_Offset;
    if (ioreq->io_Command & TDF_EXTCOM) {
	tdioreq->iotd_Count = ((struct IOExtTD *)ioreq)->iotd_Count;
	tdioreq->iotd_SecLabel = ((struct IOExtTD *)ioreq)->iotd_SecLabel;
    }
    BeginIO((struct IORequest *)tdioreq);
    WaitIO((struct IORequest *)tdioreq);

    ioreq->io_Error = tdioreq->iotd_Req.io_Error;
    ioreq->io_Actual = tdioreq->iotd_Req.io_Actual;

    TermIO(ioreq);
}
