/*-
 * $Id: support.c,v 1.55 1993/12/30 23:02:45 Rhialto Rel $
 * $Log: support.c,v $
 * Revision 1.55  1993/12/30  23:02:45	Rhialto
 * Add a few packet names.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:24:40  Rhialto
 * Add 2.0 stuff for debugging. Rearrange some #includes for precompilation.
 *
 * Revision 1.51  92/04/17  15:36:18  Rhialto
 * Freeze for MAXON.
 *
 * Revision 1.46  91/10/06  18:25:58  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.43  91/09/28  01:43:35  Rhialto
 * *** empty log message ***
 *
 * Revision 1.42  91/06/13  23:55:51  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:46:02  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.30  90/06/04  23:16:41  Rhialto
 * Release 1 Patch 3
 *
-*/

#include "han.h"
#include "dos.h"
#include <stdlib.h>

#if HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

Prototype void returnpacket(struct DosPacket *packet);
Prototype struct DosPacket *taskwait(struct Process *myproc);
Prototype long packetsqueued(void);
Prototype void *dosalloc(ulong bytes);
Prototype void dosfree(ulong *ptr);
Prototype void btos(byte *bstr, byte *buf);
Prototype void *GetHead(struct MinList *list);
Prototype void *GetTail(struct MinList *list);
Prototype char *typetostr(long ty);

/*
 * PACKET ROUTINES.	Dos Packets are in a rather strange format as you
 * can see by this and how the PACKET structure is extracted in the
 * GetMsg() of the main routine.
 */

void
returnpacket(packet)
struct DosPacket *packet;
{
    struct Message *mess;
    struct MsgPort *replyport;

    replyport = packet->dp_Port;
    mess = packet->dp_Link;
    packet->dp_Port = DosPort;
    mess->mn_Node.ln_Name = (char *) packet;
    mess->mn_Node.ln_Succ = NULL;
    mess->mn_Node.ln_Pred = NULL;
    PutMsg(replyport, mess);
}

#if TASKWAIT

/*
 * taskwait() ... Waits for a message to arrive at your port and
 *   extracts the packet address which is returned to you.
 */

typedef struct Message *(*funcptr)(__A0 void *);

struct DosPacket *
taskwait(myproc)
struct Process *myproc;
{
    struct MsgPort *myport;
    struct Message *mymess;

    if (myproc->pr_PktWait) {
	debug(("taskwait external...\n"));
	/* As per AmigaDOS tech. ref. man (V1.1) page 265: */
	/* ``In the same way as GetMsg, the function should */
	/*   return a message when one is available.'' */
	/* The same sentence is in TADM 3rd ed on page 385. */
	mymess = (*(funcptr)myproc->pr_PktWait)(myproc);
    } else {
	debug(("taskwait...\n"));
	myport = &myproc->pr_MsgPort;
	WaitPort(myport);
	mymess = (struct Message *)GetMsg(myport);
    }
    debug(("taskwait: done\n"));
    return((struct DosPacket *)mymess->mn_Node.ln_Name);
}

#endif

/*
 * Are there any packets queued to our device?
 */

long
packetsqueued()
{
    return ((void *) DosPort->mp_MsgList.lh_Head !=
	    (void *) &DosPort->mp_MsgList.lh_Tail);	/* & inserted by OIS */
}

/*
 * DOS MEMORY ROUTINES
 */

void	       *
dosalloc(bytes)
ulong	bytes;
{
    ulong *ptr;

    bytes += sizeof (*ptr);
    if (ptr = AllocMem(bytes, MEMF_PUBLIC | MEMF_CLEAR)) {
	*ptr = bytes;
	return (ptr + 1);
    }

    return NULL;
}

void
dosfree(ptr)
ulong *ptr;
{
    --ptr;
    FreeMem(ptr, *ptr);
}

/*
 * Convert a BSTR into a normal string.. copying the string into buf. I
 * use normal strings for internal storage, and convert back and forth
 * when required.
 */

void
btos(bstr, buf)
byte	       *bstr;
byte	       *buf;
{
    bstr = BTOC(bstr);
    if (*bstr)
	CopyMem(bstr + 1, buf, (long)*bstr);
    buf[*bstr] = 0;
}

/*
 * Some EXEC list handling routines not found in the EXEC library.
 */

#ifdef notdef
Prototype void * NextNode(struct MinNode *node);

void	       *
NextNode(node)
struct MinNode *node;
{
    node = node->mln_Succ;
    if (node->mln_Succ == NULL)
	return (NULL);
    return (node);
}

#endif

void	       *
GetHead(list)
struct MinList *list;
{
    if ((void *) list->mlh_Head != (void *) &list->mlh_Tail)
	return (list->mlh_Head);
    return (NULL);
}

void	       *
GetTail(list)
struct MinList *list;
{
    if ((void *) list->mlh_Head != (void *) &list->mlh_Tail)
	return (list->mlh_TailPred);
    return (NULL);
}

#if HDEBUG
char	       *
typetostr(ty)
long ty;
{
    switch (ty) {

    /* 1.3 stuff: */

    case ACTION_NIL:		    /* 0 */
	return ("ACTION_NIL");
    case ACTION_DIE:		    /* 5 */
	return ("ACTION_DIE");
    case ACTION_EVENT:		    /* 6 */
	return ("ACTION_EVENT");
    case ACTION_CURRENT_VOLUME:     /* 7 */
	return ("ACTION_CURRENT_VOLUME");
    case ACTION_LOCATE_OBJECT:	    /* 8 */
	return ("ACTION_LOCATE_OBJECT");
    case ACTION_RENAME_DISK:	    /* 9 */
	return ("ACTION_RENAME DISK");
    case ACTION_FREE_LOCK:	    /* 15 */
	return ("ACTION_FREE_LOCK");
    case ACTION_DELETE_OBJECT:	    /* 16 */
	return ("ACTION_DELETE_OBJECT");
    case ACTION_RENAME_OBJECT:	    /* 17 */
	return ("ACTION_RENAME_OBJECT");
    case ACTION_MORE_CACHE:	    /* 18 */
	return ("ACTION_MORE_CACHE");
    case ACTION_COPY_DIR:	    /* 19 */
	return ("ACTION_COPY_DIR");
    case ACTION_WAIT_CHAR:	    /* 20 */
	return ("ACTION_WAIT_FOR_CHAR");
    case ACTION_SET_PROTECT:	    /* 21 */
	return ("ACTION_SET_PROTECT");
    case ACTION_CREATE_DIR:	    /* 22 */
	return ("ACTION_CREATEDIR");
    case ACTION_EXAMINE_OBJECT:     /* 23 */
	return ("ACTION_EXAMINE OBJ");
    case ACTION_EXAMINE_NEXT:	    /* 24 */
	return ("EXAMINE NEXT");
    case ACTION_DISK_INFO:	    /* 25 */
	return ("ACTION_DISK INFO");
    case ACTION_INFO:		    /* 26 */
	return ("ACTION_INFO");
    case ACTION_FLUSH:		    /* 27 */
	return ("ACTION_FLUSH");
    case ACTION_SET_COMMENT:	    /* 28 */
	return ("ACTION_SET_COMMENT");
    case ACTION_PARENT: 	    /* 29 */
	return ("ACTION_PARENT");
    case ACTION_TIMER:		    /* 30 */
	return ("ACTION_TIMER");
    case ACTION_INHIBIT:	    /* 31 */
	return ("ACTION_INHIBIT");
    case ACTION_SET_DATE:	    /* 34 */
	return ("ACTION_SET_DATE");
    case ACTION_READ:		    /* 82 */
	return ("ACTION_READ");
    case ACTION_WRITE:		    /* 85 */
	return ("ACTION_WRITE");
    case ACTION_SCREEN_MODE:	    /* 994 */
	return ("ACTION_SCREEN_MODE");
    case ACTION_FINDUPDATE:	    /* 1004 */
	return ("ACTION_FINDUPDATE");
    case ACTION_FINDINPUT:	    /* 1005 */
	return ("ACTION_FINDINPUT");
    case ACTION_FINDOUTPUT:	    /* 1006 */
	return ("ACTION_FINDOUTPUT");
    case ACTION_END:		    /* 1007 */
	return ("ACTION_END");
    case ACTION_SEEK:		    /* 1008 */
	return ("ACTION_SEEK");

    /* 2.0 stuff: */

    case ACTION_SAME_LOCK:	    /* 40 */
	return "ACTION_SAME_LOCK";
    case ACTION_CHANGE_SIGNAL:	    /* 995 */
	return "ACTION_CHANGE_SIGNAL";
    case ACTION_FORMAT: 	    /* 1020 */
	return "ACTION_FORMAT";
    case ACTION_MAKE_LINK:	    /* 1021 */
	return "ACTION_MAKE_LINK";
    case ACTION_SET_FILE_SIZE:	    /* 1022 */
	return "ACTION_SET_FILE_SIZE";
    case ACTION_WRITE_PROTECT:	    /* 1023 */
	return "ACTION_WRITE_PROTECT";
    case ACTION_READ_LINK:	    /* 1024 */
	return "ACTION_READ_LINK";
    case ACTION_FH_FROM_LOCK:	    /* 1026 */
	return "ACTION_FH_FROM_LOCK";
    case ACTION_IS_FILESYSTEM:	    /* 1027 */
	return "ACTION_IS_FILESYSTEM";
    case ACTION_CHANGE_MODE:	    /* 1028 */
	return "ACTION_CHANGE_MODE";
    case ACTION_COPY_DIR_FH:	    /* 1030 */
	return "ACTION_COPY_DIR_FH";
    case ACTION_PARENT_FH:	    /* 1031 */
	return "ACTION_PARENT_FH";
    case ACTION_EXAMINE_ALL:	    /* 1033 */
	return "ACTION_EXAMINE_ALL";
    case ACTION_EXAMINE_FH:	    /* 1034 */
	return "ACTION_EXAMINE_FH";
#if defined(ACTION_DIRECT_READ)
    case ACTION_DIRECT_READ:	    /* 1900 CDTV */
	return "ACTION_DIRECT_READ";
#endif
    case ACTION_LOCK_RECORD:	    /* 2008 */
	return "ACTION_LOCK_RECORD";
    case ACTION_FREE_RECORD:	    /* 2009 */
	return "ACTION_FREE_RECORD";
    case ACTION_ADD_NOTIFY:	    /* 4097 */
	return "ACTION_ADD_NOTIFY";
    case ACTION_REMOVE_NOTIFY:	    /* 4098 */
	return "ACTION_REMOVE_NOTIFY";

    /* 3.0 stuff: */

    case ACTION_EXAMINE_ALL_END:    /* 1035 */
	return "ACTION_EXAMINE_ALL_END";
    case ACTION_SET_OWNER:	    /* 1036 */
	return "ACTION_SET_OWNER";
    case ACTION_SERIALIZE_DISK:     /* 4200 */
	return "ACTION_SERIALIZE_DISK";
    case ACTION_GET_DISK_FSSM:	    /* 4201 */
	return "ACTION_GET_DISK_FSSM";
    case ACTION_FREE_DISK_FSSM:     /* 4201 */
	return "ACTION_FREE_DISK_FSSM";

    default:
	return ("---------UNKNOWN-------");
    }
}

#endif				/* HDEBUG */
