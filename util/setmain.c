/*-
 * $Id: setmain.c,v 1.53 1992/10/25 02:47:12 Rhialto Rel $
 * $Log:	setmain.c,v $
 * Revision 1.53  1992/10/25  02:47:12  Rhialto
 *
 *  The main file of the MSH settings commodity.
 *
 *  This code is (C) Copyright 1992 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#define INTUI_V36_NAMES_ONLY

#include "han.h"
#include "hanconv.h"

#ifndef LIBRARIES_GADTOOLS_H
#include <libraries/gadtools.h>
#endif
#ifndef LIBRARIES_ASL_H
#include <libraries/asl.h>
#endif
#ifndef INTUITION_GADGETCLASS_H
#include <intuition/gadgetclass.h>
#endif
#ifndef DOS_DOS_H
#include <dos/dos.h>
#endif
#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif
#ifndef DOS_DOSEXTENS_H
#include <dos/dosextens.h>
#endif
#ifndef LIBRARIES_COMMODITIES_H
#include <libraries/commodities.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/asl_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>
#include <clib/commodities_protos.h>

#include "setwindow.h"

const char	idString[] = "$VER: MSH-Set $Revision$ $Date$\r\n";

struct Library *AslBase;
struct Library *CxBase;
struct Library *IconBase;

struct PrivateInfo *Private;
char	       *HandlerName;
struct MsgPort *HandlerPort;

struct MinList	MSHList;
struct MSHNode {
    struct Node     m_Node;
    char	    m_Name[2];	/* 1 for ':', 1 for '\0' */
};

struct FileRequester *FileRequest;

const char	OkString[] = "Ok";
const char	PanicString[] = "Panic!";
const char	RCSId[] = "\0$VER: MSH-Set $Revision: 1.53 $ $Date: 92/10/25 02:47:12 $, by Olaf Seibert";
char            OkString[] = "Ok";
char            PanicString[] = "Panic!";
char            RCSId[] = "\0$VER: Messydos Settings $Revision: 1.53 $ $Date: 1992/10/25 02:47:12 $, by Olaf Seibert";
void		SetGadgetAttr(int id, ULONG tag, ULONG value);
void		UpdateGadgets(struct PrivateInfo *p);
void		DeselectHandler(void);

/* Commodities stuff: */
UBYTE	      **TTypes;
struct NewBroker NewBroker = {
    NB_VERSION,
    "MSH-Set",
    "MSH Settings",
    "Change settings of MSH devices",
    NBU_UNIQUE | NBU_NOTIFY,
    COF_SHOW_HIDE,
    0, 0, 0
};
struct MsgPort *BrokerPort;
CxObj	       *Broker;

#define EVT_HOTKEY  1

/*
 * Arg2 -> Arg2, the world turned upside down.
 */

long
dos_packet1a(struct MsgPort *port, long type, long arg2, long arg3)
{
    struct StandardPacket *sp;
    struct MsgPort *rp;

    if ((rp = CreatePort(NULL, 0)) == NULL)
	return DOSFALSE;
    if ((sp = AllocMem((long) sizeof (*sp), MEMF_PUBLIC | MEMF_CLEAR)) == NULL) {
	DeletePort(rp);
	return DOSFALSE;
    }
    sp->sp_Msg.mn_Node.ln_Name = (char *) &sp->sp_Pkt;
    sp->sp_Pkt.dp_Link = &sp->sp_Msg;
    sp->sp_Pkt.dp_Port = rp;
    sp->sp_Pkt.dp_Type = type;
    sp->sp_Pkt.dp_Arg2 = arg2;
    sp->sp_Pkt.dp_Arg3 = arg3;
    PutMsg(port, &sp->sp_Msg);
    WaitPort(rp);
    GetMsg(rp);
    arg2 = sp->sp_Pkt.dp_Arg2;
    FreeMem(sp, (long) sizeof (*sp));
    DeletePort(rp);

    return arg2;
}

struct PrivateInfo *
GetPrivateInfo(struct MsgPort *port, int lockcount)
{
    if (port) {
	struct PrivateInfo *p;

	p = (struct PrivateInfo *)
	    dos_packet1a(port, ACTION_CURRENT_VOLUME, MSH_MAGIC, lockcount);

	if ((long) p == MSH_MAGIC ||
	    p == NULL ||
	    p->Revision != PRIVATE_REVISION ||
	    p->Size != sizeof(*p)) {
	    /* Incompatible filesystem. */

	    return NULL;
	}

	return p;
    }
    return NULL;
}

/* Commodities stuff: */

void
KillCx(void)
{
    if (CxBase) {
	if (Broker) {
	    DeleteCxObjAll(Broker);
	    Broker = NULL;
	}
	if (BrokerPort) {
	    struct Message *msg;

	    while (msg = GetMsg(BrokerPort)) {
		ReplyMsg(msg);
	    }
	    DeleteMsgPort(BrokerPort);
	    BrokerPort = NULL;
	}
	CloseLibrary(CxBase);
	CxBase = NULL;
    }
    if (IconBase) {
	CloseLibrary(IconBase);
	IconBase = NULL;
    }
}

void
Die(void)
{
    Hide();

    if (FileRequest)
	FreeAslRequest(FileRequest);

    KillCx();
    if (IntuitionBase) {
	CloseLibrary(IntuitionBase);
	IntuitionBase = NULL;
    }
    if (GadToolsBase) {
	CloseLibrary(GadToolsBase);
	GadToolsBase = NULL;
    }
    if (AslBase) {
	CloseLibrary(AslBase);
	AslBase = NULL;
    }
    exit(0);
}

void
OpenAll(int argc, char **argv)
{
    extern struct Library *DOSBase;
    int 	    doshow = 1;

    NewList((struct List *) &MSHList);

    if (DOSBase->lib_Version < 37) {
	puts("Sorry, requires at least V37...");
	Die();
    }
    AslBase = OpenLibrary("asl.library", 37L);
    if (!AslBase)
	Die();

    GadToolsBase = OpenLibrary("gadtools.library", 37L);
    if (!GadToolsBase)
	Die();

    IntuitionBase = OpenLibrary("intuition.library", 37L);
    if (!IntuitionBase)
	Die();

    /*
     * The following is Commodities stuff.
     * We still run if it fails to open, but not if we can't register.
     */

    CxBase = OpenLibrary("commodities.library", 37L);

    if (CxBase) {
	CxObj	       *filter;
	long		error = CBERR_OK;
	char		popup;

	IconBase = OpenLibrary("icon.library", 37L);
	if (!IconBase)
	    goto no_cx;

	TTypes = ArgArrayInit(argc, argv);

	BrokerPort = CreateMsgPort();
	NewBroker.nb_Port = BrokerPort;
	NewBroker.nb_Pri = ArgInt(TTypes, "CX_PRIORITY", 0);

	Broker = CxBroker(&NewBroker, &error);

	filter = HotKey(ArgString(TTypes, "CX_POPKEY", "rawkey control alt m"),
			BrokerPort, EVT_HOTKEY);

	popup = *ArgString(TTypes, "CX_POPUP", "Y");

	ArgArrayDone();

	/*
	 * If something goes wrong with commodities, we still run,
	 * except when we violate the commodity uniqueness.
	 */

	if (!BrokerPort || !Broker || !filter) {
	    if (filter)
		DeleteCxObjAll(filter);
	    if (error == CBERR_DUP)
		Die();
	    KillCx();
	    goto no_cx;
	}

	AttachCxObj(Broker, filter);
	if (!CxObjError(filter)) {
	    ActivateCxObj(Broker, 1L);
	    if (popup == 'N') {
		doshow = 0;
	    }
	}
    }
no_cx:
    if (doshow)
	Show();
}

void
FindAllMSHs(void)
{
    struct DosList *dlist;

    dlist = LockDosList(LDF_DEVICES | LDF_READ);

    while (dlist = NextDosEntry(dlist, LDF_DEVICES)) {
	struct PrivateInfo *p;

	if (p = GetPrivateInfo(dlist->dol_Task, 0)) {
	    char	   *name;
	    struct MSHNode *node;

	    name = (char *) BADDR(dlist->dol_Name);
	    node = AllocVec(sizeof (*node) + name[0], MEMF_CLEAR);
	    if (node) {
		strncpy(node->m_Name, &name[1], name[0]);
		strcpy(node->m_Name + name[0], ":");

		node->m_Node.ln_Name = node->m_Name;
		AddHead((struct List *) & MSHList, &node->m_Node);
	    }
	}
    }

    UnLockDosList(LDF_DEVICES | LDF_READ);
}

void
ForgetAllMSHs(void)
{
    struct Node    *n,
		   *nn;

    n = MSHList.mlh_Head;
    while (nn = n->ln_Succ) {
	FreeVec(n);
	n = nn;
    }
    NewList((struct List *) &MSHList);
}

void
Show(void)
{
    if (MSHSettingsWnd) {
	WindowToFront(MSHSettingsWnd);
	return; 		/* already open */
    }

    if (SetupScreen())
	Die();

    if (OpenMSHSettingsWindow())
	Die();

    /* Compensate for a bug in GadToolsBox 1.3: */
    ModifyIDCMP(MSHSettingsWnd,
		MSHSettingsWnd->IDCMPFlags |
		BUTTONIDCMP | CHECKBOXIDCMP | LISTVIEWIDCMP | CYCLEIDCMP);

    if (BrokerPort) {
	OnMenu(MSHSettingsWnd, FULLMENUNUM(0, 0, NOSUB));   /* "Hide A-H" */
    }

    FindAllMSHs();
    SetGadgetAttr(GDX_HANDLERS, GTLV_Labels, (ULONG) &MSHList);
    UpdateGadgets(Private);
}

void
Hide(void)
{
    if (MSHSettingsWnd) {
	struct Message *msg;
	int		height;

	height = MSHSettingsWnd->Height -
		 MSHSettingsWnd->BorderTop -
		 MSHSettingsWnd->BorderBottom;

	/*
	 * Use a random treshold to make sure we'll open full size
	 * next time, instead of shrunk...
	 */
	if (height > 10) {
	    MSHSettingsLeft = MSHSettingsWnd->LeftEdge;
	    MSHSettingsTop = MSHSettingsWnd->TopEdge;
	    MSHSettingsWidth = MSHSettingsWnd->Width -
			       MSHSettingsWnd->BorderLeft -
			       MSHSettingsWnd->BorderRight;
	    MSHSettingsHeight = height;
	}


	while (msg = GetMsg(MSHSettingsWnd->UserPort)) {
	    ReplyMsg(msg);
	}
    }

    DeselectHandler();
    CloseMSHSettingsWindow();
    CloseDownScreen();
    ForgetAllMSHs();
}

struct Node    *
GetNode(struct MinList *l, int num)
{
    struct Node    *n,
		   *nn;

    n = l->mlh_Head;
    while (num > 0 && (nn = n->ln_Succ)) {
	num--;
	n = nn;
    }

    return n;
}

void
SetGadgetAttr(int id, ULONG tag, ULONG value)
{
    GT_SetGadgetAttrs(MSHSettingsGadgets[id], MSHSettingsWnd, NULL,
		      tag, value,
		      TAG_END);
}

void
UpdateLoadGadget(int i)
{
    SetGadgetAttr(GDX_LOAD, GA_Disabled, i == 0);
}

void
DisableGadgets(long i)
{
    SetGadgetAttr(GDX_MD40TRACK, GA_Disabled, i);
    SetGadgetAttr(GDX_BOOTJMP, GA_Disabled, i);
    SetGadgetAttr(GDX_SANITY, GA_Disabled, i);
    SetGadgetAttr(GDX_SAN_DEFAULT, GA_Disabled, i);
    SetGadgetAttr(GDX_USE_DEFAULT, GA_Disabled, i);
    SetGadgetAttr(GDX_CONVERSION, GA_Disabled, i);
    SetGadgetAttr(GDX_LOAD, GA_Disabled, i);
}

void
UpdateGadgetsDis(unsigned short i)
{
    SetGadgetAttr(GDX_SANITY, GA_Disabled, (i & CHECK_USE_DEFAULT) != 0);
    SetGadgetAttr(GDX_SAN_DEFAULT, GA_Disabled,
	    (i & (CHECK_SANITY|CHECK_USE_DEFAULT)) != CHECK_SANITY);
}

void
UpdateGadgets(struct PrivateInfo *p)
{
    if (p) {
	long		i;

	DisableGadgets(FALSE);

	i = (*p->DiskIOReq)->iotd_Req.io_Flags;

	SetGadgetAttr(GDX_MD40TRACK, GTCB_Checked, (i & IOMDF_40TRACKS) != 0);

	i = *p->CheckBootBlock;

	UpdateGadgetsDis(i);
	SetGadgetAttr(GDX_BOOTJMP, GTCB_Checked, (i & CHECK_BOOTJMP) != 0);
	SetGadgetAttr(GDX_SANITY, GTCB_Checked, (i & CHECK_SANITY) != 0);
	SetGadgetAttr(GDX_SAN_DEFAULT, GTCB_Checked, (i & CHECK_SAN_DEFAULT) != 0);
	SetGadgetAttr(GDX_USE_DEFAULT, GTCB_Checked, (i & CHECK_USE_DEFAULT) != 0);

	i = *p->DefaultConversion - ConvNone;

	SetGadgetAttr(GDX_CONVERSION, GTCY_Active, i);
	UpdateLoadGadget(i);
    } else {
	DisableGadgets(TRUE);
    }
}

void
DeselectHandler(void)
{
    if (HandlerPort) {
	GetPrivateInfo(HandlerPort, -1);
	HandlerPort = NULL;
    }
    Private = NULL;
    HandlerName = NULL;
}

struct PrivateInfo *
SelectHandler(char *name)
{
    struct MsgPort *filehandler;
    struct PrivateInfo *p;

    DeselectHandler();

    filehandler = DeviceProc(name);

    if (filehandler == 0) {
	struct EasyStruct er = {
	    sizeof er, 0, PanicString,
	    "Cannot find\n%s",
	    OkString
	};

	EasyRequest(MSHSettingsWnd, &er, NULL, name);
    }

    p = GetPrivateInfo(filehandler, 1);
    UpdateGadgets(p);
    if (p) {
	HandlerName = name;
	HandlerPort = filehandler;
    }

    Private = p;
    return p;
}

unsigned char  *
InstallTable(unsigned char **ptr, unsigned char *init)
{
    Forbid();

    if (init) {
	if (*ptr == NULL)
	    *ptr = AllocMem(256L, 0L);
	if (*ptr != NULL)
	    CopyMem(init, *ptr, 256L);
    } else if (*ptr != NULL) {
	FreeMem(*ptr, 256L);
	*ptr = NULL;
    }
    Permit();

    return *ptr;
}

void
DoLoad(long qualifier)
{
    struct FileRequester *fr;
    struct PrivateInfo *p;
    int 	    conversion;

    p = Private;
    if (!p) {
	return;
    }
    conversion = *p->DefaultConversion;
    if (conversion >= ConvFence)
	conversion = ConvNone;

    if (conversion == ConvNone ||
	conversion > p->NumConversions) {
	struct EasyStruct er = {
	    sizeof er, 0, PanicString,
	    "Incorrect conversion %lc\nfor %s:",
	    OkString
	};

	EasyRequest(MSHSettingsWnd, &er, NULL,
		    (APTR)('@'+conversion), HandlerName);

	return;
    }
    conversion -= ConvNone + 1; /* Make it 0-based */

    /* If hit with shift, unload existing table */
    if (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) {
	struct EasyStruct er = {
	    sizeof er, 0, NULL,
	    "Reset %lc-conversion table?",
	    "Ok|Cancel"
	};
	long		reply;

	reply = EasyRequest(MSHSettingsWnd, &er, NULL,
		    (APTR)('@' + ConvNone + 1 + conversion));

	if (reply) {
	    InstallTable(p->Table[conversion].to, NULL);
	    InstallTable(p->Table[conversion].from, NULL);
	}

	return;
    }
    if (FileRequest == NULL) {
	FileRequest = (struct FileRequester *)
	AllocAslRequestTags(ASL_FileRequest,
			    ASL_Hail, "Select a conversion table",
			    TAG_DONE);
    }

    if (fr = FileRequest) {
	unsigned char	table[256];
	BPTR		file;

	if (AslRequest(fr, NULL) == 0) {
	    goto end;
	}
	strncpy(table, fr->rf_Dir, sizeof (table) - 1);
	AddPart(table, fr->rf_File, sizeof (table) - 1);

	file = Open(table, MODE_OLDFILE);
	if (file == 0) {
	    struct EasyStruct er = {
		sizeof er, 0, NULL,
		"Cannot open table\n%s",
		OkString
	    };

	    EasyRequest(MSHSettingsWnd, &er, NULL, table);
	    goto end;
	}
	if (Read(file, table, sizeof (table)) == 256)
	    InstallTable(p->Table[conversion].to, table);

	if (Read(file, table, sizeof (table)) == 256)
	    InstallTable(p->Table[conversion].from, table);

	Close(file);
    end:
    }
}

int
DoGadget(struct IntuiMessage *imsg)
{
    struct Gadget  *gd;
    int 	    id;
    struct PrivateInfo *p;

    gd = (struct Gadget *) imsg->IAddress;
    id = gd->GadgetID;

    switch (id) {
    case GD_HANDLERS:
	{
	    struct Node    *n;

	    n = GetNode(&MSHList, imsg->Code);
	    SelectHandler(n->ln_Name);
	}
	return 0;
    case GD_LOAD:
	DoLoad(imsg->Qualifier);
	return 0;
    }

    p = Private;
    if (!p)
	return 0;

    /* Following all assume Private is valid */

    switch (id) {
    case GD_MD40TRACK:
	{
	    struct IOExtTD *io;

	    if (io = *p->DiskIOReq) {
		if (gd->Flags & GFLG_SELECTED) {
		    io->iotd_Req.io_Flags |= IOMDF_40TRACKS;
		} else {
		    io->iotd_Req.io_Flags &= ~IOMDF_40TRACKS;
		}
	    }
	}
	break;
    case GD_BOOTJMP:
	{
	    short *f = p->CheckBootBlock;

	    if (gd->Flags & GFLG_SELECTED) {
		*f |= CHECK_BOOTJMP;
	    } else {
		*f &= ~CHECK_BOOTJMP;
	    }
	}
	break;
    case GD_SANITY:
	{
	    short *f = p->CheckBootBlock;

	    if (gd->Flags & GFLG_SELECTED) {
		*f |= CHECK_SANITY;
	    } else {
		*f &= ~CHECK_SANITY;
	    }
	    UpdateGadgetsDis(*f);
	}
	break;
    case GD_SAN_DEFAULT:
	{
	    short *f = p->CheckBootBlock;

	    if (gd->Flags & GFLG_SELECTED) {
		*f |= CHECK_SAN_DEFAULT;
	    } else {
		*f &= ~CHECK_SAN_DEFAULT;
	    }
	}
	break;
    case GD_USE_DEFAULT:
	{
	    short *f = p->CheckBootBlock;

	    if (gd->Flags & GFLG_SELECTED) {
		*f |= CHECK_USE_DEFAULT;
	    } else {
		*f &= ~CHECK_USE_DEFAULT;
	    }
	    UpdateGadgetsDis(*f);
	}
	break;
    case GD_CONVERSION:
	*p->DefaultConversion = imsg->Code + ConvNone;
	UpdateLoadGadget(imsg->Code);
	break;
    }

    return 0;
}

void
MainLoop(void)
{
    long	    waitmask;
    long	    windowmask;
    long	    brokermask;
    long	    mask;
    int 	    done;

top:
    done = 0;

    if (MSHSettingsWnd)
	windowmask = 1L << MSHSettingsWnd->UserPort->mp_SigBit;
    else
	windowmask = 0;
    if (BrokerPort)
	brokermask = 1L << BrokerPort->mp_SigBit;
     else
	brokermask = 0;

    waitmask = windowmask | brokermask | SIGBREAKF_CTRL_C;

    while (!done) {
	mask = Wait(waitmask);
	if (mask & SIGBREAKF_CTRL_C) {
	    done = 1;
	    break;
	}
	/*
	 * Note that this part won't be reachable if
	 * commodities failed to open.
	 */
	if (mask & brokermask) {
	    CxMsg	   *cxmsg;

	    while (cxmsg = GetMsg(BrokerPort)) {
		int		id;
		int		type;

		id = CxMsgID(cxmsg);
		type = CxMsgType(cxmsg);
		ReplyMsg((struct Message *)cxmsg);

		switch (type) {
		case CXM_IEVENT:
		    if (id == EVT_HOTKEY) {
			goto show;
		    }
		    break;
		case CXM_COMMAND:
		    switch (id) {
		    case CXCMD_DISABLE:
			ActivateCxObj(Broker, 0L);
			break;
		    case CXCMD_ENABLE:
			ActivateCxObj(Broker, 1L);
			break;
		    case CXCMD_APPEAR:
		    case CXCMD_UNIQUE:
		    show:
			Show();
			goto top;
		    case CXCMD_DISAPPEAR:
		    hide:
			Hide();
			goto top;
		    case CXCMD_KILL:
			done = 1;
			break;
		    }
		    break;
		}
	    }
	}
	if (mask & windowmask) {
	    struct IntuiMessage *imsg;
	    struct IntuiMessage msg;

	    while (imsg = GT_GetIMsg(MSHSettingsWnd->UserPort)) {
		msg = *imsg;
		GT_ReplyIMsg(imsg);

		switch (msg.Class) {
		case IDCMP_CLOSEWINDOW:
		    /*
		     * Commodities only hide when you try to close them.
		     */
		    if (BrokerPort) {
			goto hide;
		    } else {
			done = 1;
		    }
		    break;
		case IDCMP_GADGETUP:
		    done = DoGadget(&msg);
		    break;
		case IDCMP_MENUPICK:
		    switch (ITEMNUM(msg.Code)) {
		    case 0:	/* Hide */
			goto hide;
			break;
		    case 1:	/* Quit */
			done = 1;
			break;
		    }
		    break;
		case IDCMP_REFRESHWINDOW:
		    GT_BeginRefresh(MSHSettingsWnd);
		    GT_EndRefresh(MSHSettingsWnd, TRUE);
		    break;
		case IDCMP_DISKINSERTED:
		    SelectHandler(HandlerName);
		    UpdateGadgets(Private);
		    break;
		}
	    }
	}
    }
}

int
main(int argc, char **argv)
{
    OpenAll(argc, argv);

    MainLoop();
    Die();

    return 0;
}

#ifndef WORKBENCH_STARTUP_H
#include <workbench/startup.h>
#endif

int
wbmain(struct WBStartup *wbs)
{
    int 	    rc;
    long	    lock;

    if (wbs->sm_ArgList)
	lock = CurrentDir(wbs->sm_ArgList->wa_Lock);
    rc = main(0, (char **)wbs);
    if (wbs->sm_ArgList)
	CurrentDir(lock);
    return rc;
}
