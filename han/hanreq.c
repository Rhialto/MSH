/*-
 * $Id$
 * $Log$
 *
 *  HANREQ.C
 *
 *  The code for the messydos file system handler.
 *
 *  Read/Write error requesters.
 *
 *  This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 *  not be used or copied without a licence.
-*/

#include "dos.h"
#include <exec/io.h>
#include <devices/trackdisk.h>
#include <intuition/intuition.h>

#ifdef DEBUG
#   define	debug(x)  dbprintf x
#else
#   define	debug(x)
#endif

struct IntuiText Positive = {
    AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE, AUTOTOPEDGE, AUTOITEXTFONT,
    (UBYTE *)"Retry",
    AUTONEXTTEXT
};

struct IntuiText Negative = {
    AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE, AUTOTOPEDGE, AUTOITEXTFONT,
    (UBYTE *)"Cancel",
    AUTONEXTTEXT
};

struct IntuiText RwError[] = {
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      5,	   AUTOITEXTFONT,
	(UBYTE *)"Messydos volume",
	&RwError[1]
    },
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      15,	   AUTOITEXTFONT,
	(UBYTE *)NULL,
	&RwError[2]
    },
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      25,	   AUTOITEXTFONT,
	(UBYTE *)"has a Read or Write error",
	NULL
    },
};

struct IntuiText MustReplace[] = {
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      5,	   AUTOITEXTFONT,
	(UBYTE *)"You MUST!! replace messy volume",
	&MustReplace[1]
    },
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      15,	   AUTOITEXTFONT,
	(UBYTE *)NULL,
	&MustReplace[2]
    },
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      25,	   AUTOITEXTFONT,
	(UBYTE *)"in that floppy drive !!",
	NULL
    },
};

long AutoRequest();
extern struct DosPacket *DosPacket;
extern struct DeviceList *VolNode;
extern short DiskChanged;

long
RetryRwError(req)
struct IOExtTD *req;
{
    register struct Window *window;
    struct MsgPort *port;
    struct Process *proc;
    struct IntuiText *text;
    long result;

    if (DosPacket != NULL) { /* A user-requested action */
	port = DosPacket->dp_Port;
	if ((port->mp_Flags & PF_ACTION) != PA_SIGNAL)
	    goto fail;
	proc = (struct Process *)port->mp_SigTask;
	if (proc->pr_Task.tc_Node.ln_Type != NT_PROCESS)
	    goto fail;
	window = (struct Window *)proc->pr_WindowPtr;
	if (window == (struct Window *)-1)
	    goto fail;
    } else
	window = NULL;

    if (req->iotd_Req.io_Error == TDERR_DiskChanged) {
	text = MustReplace;
	DiskChanged = 0;
    } else
	text = RwError;

    if (VolNode)
	text[1].IText = (UBYTE *)BTOC(VolNode->dl_Name)+1;
    else
	text[1].IText = NULL;

again:
    result = AutoRequest(window, text, &Positive, &Negative,
		       0L, 0L, 320L, 72L);

    if (req->iotd_Req.io_Error == TDERR_DiskChanged && result != FALSE) {
	if (DiskChanged == 0)   /* Cheating, huh ? */
	    goto again;
	TDChangeNum();  /* Get new disk change number */
	DiskChanged = 0;
    }

    return result;

fail:
    return FALSE;
}
