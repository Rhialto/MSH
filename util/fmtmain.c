/*-
 * $Id: fmtmain.c,v 1.53 92/10/25 02:48:07 Rhialto Rel $
 * $Log:	fmtmain.c,v $
 * Revision 1.53  92/10/25  02:48:07  Rhialto
 * Initial revision.
 *
 *  FMT MAIN.C
 *
 *  The main file of the MSH formatting utility.
 *
 *  This code is (C) Copyright 1992 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#define INTUI_V36_NAMES_ONLY

#include "han.h"

#ifndef LIBRARIES_GADTOOLS_H
#include <libraries/gadtools.h>
#endif
#ifndef INTUITION_GADGETCLASS_H
#include <intuition/gadgetclass.h>
#endif
#ifndef DOS_DOS_H
#include <dos/dos.h>
#endif
#ifndef DOS_DOSEXTENS_H
#include <dos/dosextens.h>
#endif
#ifndef DOS_FILEHANDLER_H
#include <dos/filehandler.h>
#endif
#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>

#include "fmtwindow.h"

struct Library *GadToolsBase;
struct Library *IntuitionBase;

struct MinList	MSHList;
struct MSHNode {
    struct Node     m_Node;
    struct DosList *m_DosList;
    char	    m_Name[2];	/* 1 for ':', 1 for '\0' */
};

struct DosList *HandlerDList;
char	       *HandlerName;
char	       *DevName;
long		DevUnit;
unsigned long	DevFlags;

const char	OkString[] = "Ok";
const char	AbortString[] = "Abort";
const char	PanicString[] = "Panic!";
const char	RCSId[] = "\0$VER: MSH-Format $Revision: 1.53 $ $Date: 92/10/25 02:48:07 $, by Olaf Seibert";

void		Show(void);
void		Hide(void);
void		HideParm(void);
void		SetGadgetAttrM(int id, ULONG tag, ULONG value);
void		SetGadgetAttrP(int id, ULONG tag, ULONG value);
void		SetStringDefaults(void);
void		UpdateStrings(void);

#define DONE_DONE   1
#define DONE_RESET  2
#define DONE_HIDEP  4

const ulong	BootBlock[] = {
    0xEB349049, 0x424D2020, 0x332E3200, 0x02020100,	/* ...IBM  3.2..... */
    0x027000A0, 0x05F90300, 0x09000200, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x0000000F,
    0x00000000, 0x0100FA33, 0xC08ED0BC, 0x007C1607,
    0xBB780036, 0xC5371E56, 0x1653BF2B, 0x7CB90B00,
    0xFCAC2680, 0x3D007403, 0x268A05AA, 0x8AC4E2F1,
    0x061F8947, 0x02C7072B, 0x7CFBCD13, 0x7267A010,
    0x7C98F726, 0x167C0306, 0x1C7C0306, 0x0E7CA33F,
    0x7CA3377C, 0xB82000F7, 0x26117C8B, 0x1E0B7C03,
    0xC348F7F3, 0x0106377C, 0xBB0005A1, 0x3F7CE896,
    0x00B80102, 0xE8AA0072, 0x198BFBB9, 0x0B00BECD,
    0x7DF3A675, 0x0D8D7F20, 0xBED87DB9, 0x0B00F3A6,
    0x7418BE6E, 0x7DE86100, 0x32E4CD16, 0x5E1F8F04,
    0x8F4402CD, 0x19BEB77D, 0xEBEBA11C, 0x0533D2F7,
    0x360B7CFE, 0xC0A23C7C, 0xA1377CA3, 0x3D7CBB00,
    0x07A1377C, 0xE84000A1, 0x187C2A06, 0x3B7C4050,
    0xE84E0058, 0x72CF2806, 0x3C7C760C, 0x0106377C,
    0xF7260B7C, 0x03D8EBD9, 0x8A2E157C, 0x8A16FD7D,
    0x8B1E3D7C, 0xEA000070, 0x00AC0AC0, 0x7422B40E,
    0xBB0700CD, 0x10EBF233, 0xD2F73618, 0x7CFEC288,
    0x163B7C33, 0xD2F7361A, 0x7C88162A, 0x7CA3397C,
    0xC3B4028B, 0x16397CB1, 0x06D2E60A, 0x363B7C8B,
    0xCA86E98A, 0x16FD7D8A, 0x362A7CCD, 0x13C30D0A,
    0x4E6F6E2D, 0x53797374, 0x656D2064, 0x69736B20,	/* Non-System disk  */
    0x6F722064, 0x69736B20, 0x6572726F, 0x720D0A52,	/* or disk error..R */
    0x65706C61, 0x63652061, 0x6E642073, 0x7472696B,	/* eplace and strik */
    0x6520616E, 0x79206B65, 0x79207768, 0x656E2072,	/* e any key when r */
    0x65616479, 0x0D0A000D, 0x0A446973, 0x6B20426F,	/* eady.....Disk Bo */
    0x6F742066, 0x61696C75, 0x72650D0A, 0x0049424D,	/* ot failure...IBM */
    0x42494F20, 0x20434F4D, 0x49424D44, 0x4F532020,	/* BIO	COMIBMDOS   */
    0x434F4D00, 0x00000000, 0x00000000, 0x00000000,	/* COM............. */
    0x00000000, 0x00000000, 0x00000000, 0x000055AA,
};

byte	       *DiskTrack;
long		TrackSize;
int		Track;
int		EndTrack;
struct IOExtTD *TDReq;
int		Break;

int		Parameters[13];
int		FormatWhat;
#define BOOT_ONLY   0
#define BOOT_ROOT   1
#define WHOLE_DISK  2

#define BPS	Parameters[GDX_BPS]
#define SPT	Parameters[GDX_SPT]
#define NSIDES	Parameters[GDX_NSIDES]
#define FIRSTCYL Parameters[GDX_FIRSTCYL]
#define NCYLS	Parameters[GDX_NCYLS]
#define SPC	Parameters[GDX_SPC]
#define RESERVED Parameters[GDX_RESERVED]
#define NFATS	Parameters[GDX_NFATS]
#define NDIRS	Parameters[GDX_NDIRS]
#define NSECTS	Parameters[GDX_NSECTS]
#define MEDIA	Parameters[GDX_MEDIA]
#define SPF	Parameters[GDX_SPF]
#define NHID	Parameters[GDX_NHID]

int
todigit(char   c)
{
    if ((c -= '0') < 0 || c > ('F' - '0'))
	return 42;

    if (c >= ('A' - '0')) {
	c -= 'A' - '9' - 1;
    }
    return c;
}

long
ntoi(char  *str)
{
    long	    total = 0;
    long	    value;
    int 	    digit;
    int 	    base;

    /* First determine the base */
number:
    if (*str == '0') {
	if (*++str == 'x') {    /* 0x means hexadecimal */
	    base = 16;
	    str++;
	} else {
	    base = 8;		/* Otherwise, 0 means octal */
	}
    } else {
	base = 10;		/* and any other digit means decimal */
    }

    value = 0;
    while ((digit = todigit(*str)) < base) {
	value *= base;
	value += digit;
	str++;
    }

suffix:
    switch (*str++) {
    case 'm':                   /* scale with megabytes */
	value *= 1024L * 1024;
	goto suffix;
    case 'k':                   /* scale with kilobytes */
	value *= 1024;
	goto suffix;
    case 's':                   /* scale with sectors */
	value *= TD_SECTOR;	/* or maybe even kilosectors! */
	goto suffix;
    case 'b':                   /* scale with bytes */
	goto suffix;
    }
    str--;

    total += value;

    if (*str >= '0' && *str <= '9')
	goto number;		/* Allow 10k512, recursion eliminated */

    return total;
}

char *
iton(char *string, int size, int value)
{
    if (value >= 1024 * 1024L) {
	int		m;
	int		rest;
	char	       *last;

	m = value / (1024 * 1024L);
	rest = value - (m * 1024L * 1024);
	last = iton(string, size, m);
	*last++ = 'm';
	size -= (last - string);
	value = rest;
	string = last;
    }

    if (value >= 64 * 1024) {
	int		k;
	int		rest;
	char	       *last;

	k = value / 1024;
	rest = value - (k * 1024);
	last = iton(string, size, k);
	*last++ = 'k';
	size -= (last - string);
	value = rest;
	string = last;
    }
    sprintf(string, "%d", value);
    while (*string)
	string++;

    return string;
}


void
PutWord(byte *address, word value)
{
    address[0] = value;
    address[1] = value >> 8;
}


void
Die(void)
{
    Hide();

    if (IntuitionBase) {
	CloseLibrary(IntuitionBase);
	IntuitionBase = NULL;
    }
    if (GadToolsBase) {
	CloseLibrary(GadToolsBase);
	GadToolsBase = NULL;
    }
    exit(0);
}

void
OpenAll(int argc, char **argv)
{
    extern struct Library *DOSBase;

    NewList((struct List *) &MSHList);

    if (DOSBase->lib_Version < 37) {
	puts("Sorry, requires at least V37...");
	Die();
    }
    GadToolsBase = OpenLibrary("gadtools.library", 37L);
    if (!GadToolsBase)
	Die();

    IntuitionBase = OpenLibrary("intuition.library", 37L);
    if (!IntuitionBase)
	Die();

    Show();
}

int
isFileSystem(struct MsgPort *handler)
{
    if (handler == NULL)
	return FALSE;

    if (DoPkt(handler, ACTION_IS_FILESYSTEM, 0, 0, 0, 0, 0)) {
	return TRUE;
    }
    return FALSE;
}

void
FindAllDevices(void)
{
    struct DosList *dlist;

    dlist = LockDosList(LDF_DEVICES | LDF_READ);

    while (dlist = NextDosEntry(dlist, LDF_DEVICES)) {
	if (dlist->dol_misc.dol_handler.dol_Startup > 100 &&
	    isFileSystem(dlist->dol_Task)) {
	    char	   *name;
	    struct MSHNode *node;

	    name = (char *) BADDR(dlist->dol_Name);
	    node = AllocVec(sizeof (*node) + name[0], MEMF_CLEAR);
	    if (node) {
		strncpy(node->m_Name, &name[1], name[0]);
		strcpy(node->m_Name + name[0], ":");

		node->m_Node.ln_Name = node->m_Name;
		node->m_DosList = dlist;
		AddHead((struct List *) &MSHList, &node->m_Node);
	    }
	}
    }

    UnLockDosList(LDF_DEVICES | LDF_READ);
}

void
ForgetAllDevices(void)
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
    if (MainWnd) {
	WindowToFront(MainWnd);
	return; 		/* already open */
    }

    if (SetupScreen())
	Die();

    if (OpenMainWindow())
	Die();

    /* Compensate for a bug in GadToolsBox 1.3: */
    ModifyIDCMP(MainWnd,
		MainWnd->IDCMPFlags |
		MXIDCMP | STRINGIDCMP | LISTVIEWIDCMP | BUTTONIDCMP);

    FindAllDevices();
    SetGadgetAttrM(GDX_HANDLERS, GTLV_Labels, (ULONG) &MSHList);
}

void
ShowParm(void)
{
    if (ParmWnd) {
	WindowToFront(ParmWnd);
	return; 		/* already open */
    }

    if (OpenParmWindow())
	return;

    /* Compensate for a bug in GadToolsBox 1.3: */
    ModifyIDCMP(ParmWnd,
		ParmWnd->IDCMPFlags | STRINGIDCMP | BUTTONIDCMP);

    UpdateStrings();
}

void
Hide(void)
{
    HideParm();

    if (MainWnd) {
	struct IntuiMessage *msg;

	while (msg = GT_GetIMsg(MainWnd->UserPort)) {
	    GT_ReplyIMsg(msg);
	}
    }

    CloseMainWindow();
    CloseDownScreen();
    ForgetAllDevices();
}

void
HideParm(void)
{
#if 0
    if (ParmWnd) {
	struct IntuiMessage *msg;

	while (msg = GT_GetIMsg(ParmWnd->UserPort)) {
	    GT_ReplyIMsg(msg);
	}
    }
#endif
    CloseParmWindow();
}

struct MSHNode	   *
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
DeselectHandler(void)
{
    HandlerDList = NULL;
}

void
SelectHandler(struct MSHNode *node)
{
    struct DosList *dl;
    struct FileSysStartupMsg *fssm;

    DeselectHandler();

    HandlerName = node->m_Node.ln_Name;
    dl = node->m_DosList;
    HandlerDList = dl;
    fssm = (struct FileSysStartupMsg *)BADDR(dl->dol_misc.dol_handler.dol_Startup);
    DevName = ((char *)BADDR(fssm->fssm_Device)) + 1;
    DevFlags = fssm->fssm_Flags;
    DevUnit = fssm->fssm_Unit;
    SetStringDefaults();
}

void
SetGadgetAttrM(int id, ULONG tag, ULONG value)
{
    GT_SetGadgetAttrs(MainGadgets[id], MainWnd, NULL,
		      tag, value,
		      TAG_END);
}

void
SetGadgetAttrP(int id, ULONG tag, ULONG value)
{
    if (ParmWnd) {
	GT_SetGadgetAttrs(ParmGadgets[id], ParmWnd, NULL,
			  tag, value,
			  TAG_END);
    }
}

void
UpdateString(int id)
{
    char	    newstring[80];

    iton(newstring, sizeof newstring, Parameters[id]);
    SetGadgetAttrP(id, GTST_String, (ULONG)newstring);
}

void
UpdateStrings(void)
{
    int 	    i;

    if (!ParmWnd) {
	return;
    }

    for (i = 0; i < GDX_OK; i++) {      /* 0 == GDX_BPS */
	UpdateString(i);
    }
}

void
MotorOff(void)
{
    TDReq->iotd_Req.io_Command = TD_MOTOR;
    TDReq->iotd_Req.io_Length = 0;
    DoIO((struct IORequest *)TDReq);
}

int
IgnoreAbortRetry(char *fmt, ...)
{
    static struct EasyStruct er0 = {
	sizeof er0, 0, PanicString,
	NULL,
	"Abort|Retry|Ignore"
    };
    struct EasyStruct er;
    va_list	    va;

    MotorOff();
    er = er0;
    er.es_TextFormat = fmt;
    va_start(va, fmt);
      return EasyRequestArgs(MainWnd, &er, NULL, va);
    va_end(va)          /* produces error if it doesn't expand to empty */
}

int
Confirm(char *fmt, char *buttons, ...)
{
    static struct EasyStruct er0 = {
	sizeof er0, 0, "MSH Format",
	NULL,
	NULL,
    };
    struct EasyStruct er;
    va_list	    va;

    er = er0;
    er.es_TextFormat = fmt;
    er.es_GadgetFormat = buttons;
    va_start(va, buttons);
      return EasyRequestArgs(MainWnd, &er, NULL, va);
    va_end(va)          /* produces error if it doesn't expand to empty */
}

/*
 * Look through incoming messages to see if the user is playing
 * with the gadgets. If so, ask if she wants to stop formatting.
 */

int
CheckCancel(void)
{
    if (MainWnd) {
	struct IntuiMessage *msg, *next;

	msg = (struct IntuiMessage *)MainWnd->UserPort->mp_MsgList.lh_Head;
	while (next = (struct IntuiMessage *)msg->ExecMessage.mn_Node.ln_Succ) {
	    if (msg->Class & (IDCMP_GADGETUP | IDCMP_GADGETDOWN |
			      IDCMP_CLOSEWINDOW)) {
		Remove(&msg->ExecMessage.mn_Node);
		ReplyMsg(&msg->ExecMessage);

		MotorOff();
		Break = Confirm("Stop formatting?", "Stop|Continue");
		return Break;
	    }
	    msg = next;
	}
    }

    return 0;
}

void
Title(char *fmt, int t, int s)
{
    static char buf[80];

    sprintf(buf, fmt, t, s);
    SetWindowTitles(MainWnd, buf, (char *)-1);
}

byte	       *
MaybeWrite(byte *block)
{
    while (Break == 0 &&
	   block >= (DiskTrack + TrackSize)) {
	char	       *msg;
	int		t,
			s;

	t = Track / NSIDES;
	s = Track % NSIDES;
    top:
	CheckCancel();
	if (Break)
	    break;
	Title("Writing cylinder %3d side %d...", t, s);
	TDReq->iotd_Req.io_Command = TD_FORMAT;
	TDReq->iotd_Req.io_Data = (APTR) DiskTrack;
	TDReq->iotd_Req.io_Length = TrackSize;
	TDReq->iotd_Req.io_Offset = TrackSize * Track;
	DoIO((struct IORequest *)TDReq);
	if (TDReq->iotd_Req.io_Error) {
	    msg = "Write";
	    goto error;
	}
	TDReq->iotd_Req.io_Command = CMD_UPDATE;
	DoIO((struct IORequest *)TDReq);
	if (TDReq->iotd_Req.io_Error) {
	    msg = "Update";
	    goto error;
	}
	TDReq->iotd_Req.io_Command = CMD_CLEAR;
	DoIO((struct IORequest *)TDReq);

	Title("Reading cylinder %3d side %d...", t, s);
	TDReq->iotd_Req.io_Command = CMD_READ;
	TDReq->iotd_Req.io_Data = (APTR) DiskTrack;
	TDReq->iotd_Req.io_Length = TrackSize;
	TDReq->iotd_Req.io_Offset = TrackSize * Track;
	DoIO((struct IORequest *)TDReq);
	if (TDReq->iotd_Req.io_Error) {
	    msg = "Read";
	error:
	    Break = IgnoreAbortRetry("%s error %ld on\ncylinder %ld side %ld.\n",
		   msg, TDReq->iotd_Req.io_Error, t, s);
	    if (Break == 2) {
		Break = 0;
		goto top;
	    }
	}
	setmem(DiskTrack, (int) TrackSize, 0);
	Track++;
	if ((block -= TrackSize) < DiskTrack)
	    block = DiskTrack;
    }
    return block;
}

int
DoFormat(void)
{
    struct MsgPort *port;
    byte	   *diskBlock;
    char	   *title;
    int 	    i;

    Break = 0;
    title = MainWnd->Title;

    if (!(port = CreatePort(NULL, 0L))) {
	DisplayBeep(NULL);
	goto abort1;
    }
    if (!(TDReq = CreateExtIO(port, (long) sizeof (*TDReq)))) {
	DisplayBeep(NULL);
	goto abort2;
    }
    if (OpenDevice(DevName, DevUnit, (struct IORequest *)TDReq, 0L)) {
	Confirm("Cannot OpenDevice %s", AbortString, DevName);
	goto abort3;
    }

    TrackSize = BPS * SPT;
    Track = FIRSTCYL * NSIDES;
    EndTrack = Track + NSIDES * NCYLS;

    if ((DiskTrack = AllocMem(TrackSize,
			MEMF_PUBLIC | MEMF_CHIP | MEMF_CLEAR)) == NULL) {
	Confirm("No memory for track buffer\n(%ld bytes of chipmem)", NULL, TrackSize);
	goto abort4;
    }
    CopyMem((char *)BootBlock, DiskTrack, (long) sizeof (BootBlock));

    PutWord(DiskTrack + 0x0b,   BPS);
	  *(DiskTrack + 0x0d) = SPC;
    PutWord(DiskTrack + 0x0e,   RESERVED);
	  *(DiskTrack + 0x10) = NFATS;
    PutWord(DiskTrack + 0x11,   NDIRS);
    PutWord(DiskTrack + 0x13,   NSECTS);
	  *(DiskTrack + 0x15) = MEDIA;
    PutWord(DiskTrack + 0x16,   SPF);
    PutWord(DiskTrack + 0x18,   SPT);
    PutWord(DiskTrack + 0x1a,   NSIDES);
    PutWord(DiskTrack + 0x1c,   NHID);

    Break = Confirm("Insert disk to be formatted in\n%s unit %ld",
	    "Cancel|Format",
	    DevName, DevUnit);
    if (Break)
	return 1;

    if (!Inhibit(HandlerName, DOSTRUE)) {
	Confirm("Cannot inhibit %s!", AbortString, HandlerName);
	goto done;
    }

    if (FormatWhat == BOOT_ONLY) {
    top:
	Title("Writing bootblock only.", 0, 0);
	TDReq->iotd_Req.io_Command = CMD_WRITE;
	TDReq->iotd_Req.io_Data = (APTR) DiskTrack;
	TDReq->iotd_Req.io_Length = sizeof (BootBlock);
	TDReq->iotd_Req.io_Offset = 0;
	DoIO((struct IORequest *)TDReq);

	if (TDReq->iotd_Req.io_Error) {
	    Break = IgnoreAbortRetry("Write error %ld!",
		    TDReq->iotd_Req.io_Error);
	    if (Break == 2) {
		Break = 0;
		goto top;
	    }
	}
	if (!Break) {
	    TDReq->iotd_Req.io_Command = CMD_UPDATE;
	    DoIO((struct IORequest *)TDReq);
	}

	goto done;
    }

    /* Go to first FAT */
    diskBlock = MaybeWrite(DiskTrack + BPS * RESERVED);
    for (i = 0; i < NFATS; i++) {
	diskBlock[0] = 0xF9;
	diskBlock[1] = 0xFF;
	diskBlock[2] = 0xFF;
	diskBlock = MaybeWrite(diskBlock + BPS * SPF);  /* Next FAT */
    }

    /* Clear entire directory */
    diskBlock = MaybeWrite(diskBlock + NDIRS * MS_DIRENTSIZE);
    MaybeWrite(DiskTrack + TrackSize);  /* Force a write */

    if (FormatWhat == WHOLE_DISK) {
	while (Track < EndTrack) {
	    MaybeWrite(DiskTrack + TrackSize);  /* Write an empty track */
	    if (Break)
		break;
	}
    }

done:
    MotorOff();
    TDReq->iotd_Req.io_Command = TD_MOTOR;
    TDReq->iotd_Req.io_Length = 0;
    DoIO((struct IORequest *)TDReq);

    SetWindowTitles(MainWnd, title, (char *)-1);
    if (!Break)
	Confirm("Format finished.", OkString);
    Inhibit(HandlerName, DOSFALSE);

abort5:
    FreeMem(DiskTrack, TrackSize);
abort4:
    CloseDevice((struct IORequest *)TDReq);
abort3:
    DeleteExtIO((struct IORequest *)TDReq);
abort2:
    DeletePort(port);
abort1:;

    return Break;
}

void
Recalculate(int id)
{
    long	    mask;

    mask = (id < 0)? ~0 : 1L << id;

    if (mask & (1L<<GDX_SPT | 1L<<GDX_NSIDES | 1L<<GDX_NCYLS)) {
	NSECTS = SPT * NSIDES * NCYLS;
	UpdateString(GDX_NSECTS);
	mask |= 1L<<GDX_NSECTS;
    }
    if (mask & (1L<<GDX_NSECTS | 1L<<GDX_SPC | 1L<<GDX_BPS)) {
    /*
     * Suggest a minimum value for the number of FAT sectors.
     * Here we assume all sectors are to be used for clusters, which is not
     * really true, but simpler, and gives a conservative value. Besides,
     * the number of available sectors also depends on the FAT size, so
     * the whole calculation (if done correctly) would be recursive. In
     * practice, it may occasionally suggest one sector too much.
     */
	long		nclusters;
	long		nbytes;
	int		spf;

	nclusters = MS_FIRSTCLUST + (NSECTS + SPC - 1) / SPC;
	if (nclusters > 0xFF7) /* 16-bit FAT entries */
	    nbytes = nclusters * 2;
	else		      /* 12-bit FAT entries */
	    nbytes = (nclusters * 3 + 1) / 2;
	spf = (nbytes + BPS - 1) / BPS;
	/* Hack for floppies */
	if (spf < MS_SPF)
	    spf = MS_SPF;

	SPF = spf;
	UpdateString(GDX_SPF);
    }
}

void
SetStringDefaults(void)
{
    BPS 	= MS_BPS;
    SPT 	= MS_SPT;
    NSIDES	= MS_NSIDES;
    FIRSTCYL	= 0;
    NCYLS	= 80;
    RESERVED	= MS_RES;
    NHID	= 0;
    SPC 	= MS_SPC;
    NFATS	= MS_NFATS;
    MEDIA	= 0xF9;
    NDIRS	= MS_NDIRS;

    if (HandlerDList) {
	struct DosList *dl;
	struct FileSysStartupMsg *fssm;
	long	       *environ;

	dl = HandlerDList;
	fssm = (struct FileSysStartupMsg *)BADDR(dl->dol_misc.dol_handler.dol_Startup);

	if ((environ = (long *)BADDR(fssm->fssm_Environ)) &&
	    environ[DE_TABLESIZE] >= DE_UPPERCYL) {
	    NSIDES   = environ[DE_NUMHEADS];
	    SPT      = environ[DE_BLKSPERTRACK];
	    BPS      = environ[DE_SIZEBLOCK] * 4;
	    FIRSTCYL = environ[DE_LOWCYL];
	    NCYLS    = environ[DE_UPPERCYL] - FIRSTCYL + 1;
	}
    }
    Recalculate(-1);
    UpdateStrings();
}

int
DoGadget(struct IntuiMessage *imsg)
{
    struct Gadget  *gd;
    int 	    id;

    gd = (struct Gadget *) imsg->IAddress;
    id = gd->GadgetID;

    switch (id) {
    case GD_HANDLERS:
	{
	    struct MSHNode *n;

	    n = GetNode(&MSHList, imsg->Code);
	    SelectHandler(n);
	    SetGadgetAttrM(GDX_DOIT, GA_Disabled, FALSE);
	    SetGadgetAttrM(GDX_OPTIONS, GA_Disabled, FALSE);
	}
	break;
    case GD_FMT_WHAT:
	FormatWhat = imsg->Code;
	break;
    case GD_OPTIONS:
	ShowParm();
	return 2;
    case GD_DOIT:
	DoFormat();
	break;
    case GD_CANCEL:
	return DONE_DONE;


    case GD_BPS:
    case GD_SPT:
    case GD_NSIDES:
    case GD_FIRSTCYL:
    case GD_NCYLS:
    case GD_SPC:
    case GD_RESERVED:
    case GD_NFATS:
    case GD_NDIRS:
    case GD_NSECTS:
    case GD_MEDIA:
    case GD_SPF:
    case GD_NHID:
	id -= GD_BPS - GDX_BPS;     /* convert to array index */
	Parameters[id] = ntoi(((struct StringInfo *)gd->SpecialInfo)->Buffer);
	Recalculate(id);
	break;
    case GD_OK:
	return DONE_HIDEP;
    }

    return 0;
}

struct IntuiMessage *
Safe_GT_GetIMsg(struct Window *w)
{
    if (w)
	return GT_GetIMsg(w->UserPort);
    return NULL;
}

void
MainLoop(void)
{
    long	    waitmask;
    long	    mainmask;
    long	    parmmask;
    long	    mask;
    int 	    done;

top:
    done = 0;

    if (MainWnd)
	mainmask = 1L << MainWnd->UserPort->mp_SigBit;
    else
	mainmask = 0;

    if (ParmWnd)
	parmmask = 1L << ParmWnd->UserPort->mp_SigBit;
    else
	parmmask = 0;

    waitmask = mainmask | parmmask | SIGBREAKF_CTRL_C;

    while (!done) {
	mask = Wait(waitmask);
	if (mask & SIGBREAKF_CTRL_C) {
	    done = DONE_DONE;
	    break;
	}
	if (mask & (mainmask | parmmask)) {
	    struct IntuiMessage *imsg;

	    while ((imsg = Safe_GT_GetIMsg(MainWnd)) ||
		   (imsg = Safe_GT_GetIMsg(ParmWnd))) {
		switch (imsg->Class) {
		case IDCMP_CLOSEWINDOW:
		    if (imsg->IDCMPWindow == MainWnd) {
			done = DONE_DONE;
		    } else {
			done = DONE_HIDEP;
		    }
		    break;
		case IDCMP_GADGETDOWN:
		case IDCMP_GADGETUP:
		    done = DoGadget(imsg);
		    break;
		case IDCMP_REFRESHWINDOW:
		    GT_BeginRefresh(imsg->IDCMPWindow);
		    GT_EndRefresh(imsg->IDCMPWindow, TRUE);
		    break;
		}
		GT_ReplyIMsg(imsg);
		if (done & DONE_HIDEP) {
		    HideParm();
		    goto top;
		}
	    }
	}
	if (done & DONE_RESET) {
	    goto top;
	}
    }
}

void chkabort(void) {}      /* DICE specific. */

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
