/*-
 * $Id: device2.c,v 1.42 91/06/13 23:45:09 Rhialto Exp $
 * $Log:	device2.c,v $
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
 * This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
-*/

#include <amiga.h>
#include <functions.h>
#include "dev.h"
#include "device.h"

/*#undef DEBUG			/**/
#ifdef DEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif
/* INDENT ON */


char		DevName[] = "messydisk.device";
char		idString[] = "messydisk.device $Revision: 1.42 $ $Date: 91/06/13 23:45:09 $\r\n";

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

void		(*funcTable[]) (struct IOStdReq *, UNIT *) = {
    CMD_Invalid, CMD_Reset, CMD_Read, CMD_Write, CMD_Update, CMD_Clear,
    CMD_Stop, CMD_Start, CMD_Flush, TD_Motor, TD_Seek, TD_Format,
    TD_Remove, TD_Changenum, TD_Changestate, TD_Protstatus, TD_Rawread,
    TD_Rawwrite, TD_Getdrivetype, TD_Getnumtracks, TD_Addchangeint,
    TD_Remchangeint,
};


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


__stkargs __geta4 DEV		 *
CInit(D2, D3, segment)
ulong		D2,
		D3;
long		segment;
{
    DEV 	   *dev;

    SysBase = *(long *) 4;
#ifdef DEBUG
    initsyslog();
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
	AddDevice(&dev->md_Dev);
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

__stkargs __geta4 void
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

__stkargs __geta4 long
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

__stkargs __geta4 long
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
    Remove(&dev->dev_Node);
    DevCloseDown(dev);          /* Should be quick! */
#ifdef DEBUG
    uninitsyslog();
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

__stkargs __geta4 void
DevBeginIO(D2, D3, ioreq, dev)
ulong		D2,
		D3;
struct IOStdReq *ioreq;
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

__stkargs __geta4 long
DevAbortIO(D2, D3, ioreq, dev)
ulong		D2,
		D3;
struct IOStdReq *ioreq;
DEV	       *dev;
{
    if (ioreq->io_Flags & IOF_QUICK ||
	IMMEDIATE & (1L << STRIP(ioreq->io_Command))) {
	return 1;
    } else {
	Forbid();
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
