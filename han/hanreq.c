/*-
 * $Id: hanreq.c,v 1.46 91/10/06 18:25:03 Rhialto Rel $
 * $Log:	hanreq.c,v $
 * Revision 1.46  91/10/06  18:25:03  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.45  91/10/04  00:12:06  Rhialto
 * Add general information requester
 *
 * Revision 1.43  91/09/28  01:37:13  Rhialto
 * Changed to newer syslog stuff.
 *
 * Revision 1.42  91/06/13  23:47:15  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:54:07  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.32  90/11/23  23:53:42  Rhialto
 * Prepare for syslog
 *
 * Revision 1.30  90/06/04  23:17:48  Rhialto
 * Release 1 Patch 3
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

#include <amiga.h>
#include <functions.h>
#include "han.h"
#include "dos.h"
#ifndef INTUITION_INTUITION_H
#include <intuition/intuition.h>
#endif

#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

Prototype short Cancel; 	/* Cancel all R/W errors */
Prototype long	RetryRwError(struct IOExtTD *req);
Prototype void	DisplayMessage(char *msg);
Local	  struct Window *Getpr_WindowPtr(void);

short		Cancel = 0;	/* Cancel all R/W errors */

static const UBYTE retry[] = "Retry";
static const UBYTE cancel[] = "Cancel";

static const struct IntuiText Positive = {
    AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE, AUTOTOPEDGE, AUTOITEXTFONT,
    retry,
    AUTONEXTTEXT
};

static const struct IntuiText Negative = {
    AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE, AUTOTOPEDGE, AUTOITEXTFONT,
    cancel,
    AUTONEXTTEXT
};

static const UBYTE volume[] = "Messydos volume";
static const UBYTE hasrwerr[] = "has a Read or Write error";

static struct IntuiText RwError[] = {
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      5,	   AUTOITEXTFONT,
	volume,
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
	hasrwerr,
	NULL
    },
};

static const UBYTE must[] = "You MUST!! replace messy volume";
static const UBYTE inthat[] = "in that floppy drive !!";

struct IntuiText MustReplace[] = {
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      5,	   AUTOITEXTFONT,
	must,
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
	inthat,
	NULL
    },
};

static struct Window *
Getpr_WindowPtr()
{
    if (DosPacket != NULL) { /* A user-requested action */
	struct MsgPort *port;
	struct Process *proc;

	port = DosPacket->dp_Port;
	if ((port->mp_Flags & PF_ACTION) != PA_SIGNAL)
	    return (struct Window *)-1;
	proc = (struct Process *)port->mp_SigTask;
	if (proc->pr_Task.tc_Node.ln_Type != NT_PROCESS)
	    return (struct Window *)-1;
	return (struct Window *)proc->pr_WindowPtr;
    } else
	return NULL;
}

long
RetryRwError(req)
struct IOExtTD *req;
{
    struct Window  *window;
    struct IntuiText *text;
    long	    result;

    if (Cancel)
	goto fail;

    window = Getpr_WindowPtr();
    if (window == (struct Window *)-1)
	goto fail;

    if (req->iotd_Req.io_Error == TDERR_DiskChanged) {
	text = MustReplace;
	DiskChanged = 0;
    } else
	text = RwError;

    if (VolNode)
	text[1].IText = (UBYTE *)BTOC(VolNode->dl_Name)+1;
    else
	text[1].IText = (UBYTE *)"";

again:
    result = AutoRequest(window, text, &Positive, &Negative,
			 0L, 0L, 320L, 72L);

    if (req->iotd_Req.io_Error == TDERR_DiskChanged && result != FALSE) {
	TDChangeNum();  /* Get new disk change number */
	DiskChanged = 0;
    }

    return result;

fail:
    return FALSE;
}

static const UBYTE ok[] = "Ok";

static const struct IntuiText Ok = {
    AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE, AUTOTOPEDGE, AUTOITEXTFONT,
    ok,
    AUTONEXTTEXT
};

static struct IntuiText AnyMessage[] = {
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      10,	   AUTOITEXTFONT,
	(UBYTE *)NULL,
	&AnyMessage[1]
    },
    {
	AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
	16,	      25,	   AUTOITEXTFONT,
	(UBYTE *)NULL,
	NULL
    }
};

void
DisplayMessage(msg)
char	    *msg;
{
    struct Window  *window;

    window = Getpr_WindowPtr();
    if (window == (struct Window *)-1)
	window = NULL;

    AnyMessage[0].IText = (UBYTE *)BTOC(DevNode->dn_Name) + 1;
    AnyMessage[1].IText = msg;
    (void) AutoRequest(window, AnyMessage, NULL, &Ok,
		       0L, 0L, 320L, 72L);

}
