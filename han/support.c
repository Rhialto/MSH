/*-
 * $Id: support.c,v 1.42 91/06/13 23:55:51 Rhialto Exp $
 * $Log:	support.c,v $
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

#include <amiga.h>
#include <functions.h>

typedef unsigned long		ulong;
typedef unsigned char		byte;

#include "dos.h"

extern PORT    *DosPort;	/* Our DOS port... */

/*
 * PACKET ROUTINES.	Dos Packets are in a rather strange format as you
 * can see by this and how the PACKET structure is extracted in the
 * GetMsg() of the main routine.
 */

void
returnpacket(packet)
register struct DosPacket *packet;
{
    register struct Message *mess;
    register struct MsgPort *replyport;

    replyport = packet->dp_Port;
    mess = packet->dp_Link;
    packet->dp_Port = DosPort;
    mess->mn_Node.ln_Name = (char *) packet;
    mess->mn_Node.ln_Succ = NULL;
    mess->mn_Node.ln_Pred = NULL;
    PutMsg(replyport, mess);
}

/*
 * Are there any packets queued to our device?
 */

int
packetsqueued()
{
    return ((void *) DosPort->mp_MsgList.lh_Head !=
	    (void *) &DosPort->mp_MsgList.lh_Tail);     /* & inserted by OIS */
}

/*
 * DOS MEMORY ROUTINES
 */

void	       *
dosalloc(bytes)
register ulong	bytes;
{
    register ulong *ptr;

    bytes += sizeof (*ptr);
    if (ptr = AllocMem(bytes, MEMF_PUBLIC | MEMF_CLEAR)) {
	*ptr = bytes;
	return (ptr + 1);
    }

    return NULL;
}

void
dosfree(ptr)
register ulong *ptr;
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

void	       *
NextNode(node)
register NODE		*node;
{
    node = node->mln_Succ;
    if (node->mln_Succ == NULL)
	return (NULL);
    return (node);
}

#endif

void	       *
GetHead(list)
register LIST		*list;
{
    if ((void *) list->mlh_Head != (void *) &list->mlh_Tail)
	return (list->mlh_Head);
    return (NULL);
}

void	       *
GetTail(list)
register LIST		*list;
{
    if ((void *) list->mlh_Head != (void *) &list->mlh_Tail)
	return (list->mlh_TailPred);
    return (NULL);
}

#ifdef HDEBUG
char	       *
typetostr(ty)
long ty;
{
    switch (ty) {
    case ACTION_DIE:
	return ("DIE");
    case ACTION_CURRENT_VOLUME:
	return ("CURRENT VOLUME");
    case ACTION_OPENRW:
	return ("OPEN-RW");
    case ACTION_OPENOLD:
	return ("OPEN-OLD");
    case ACTION_OPENNEW:
	return ("OPEN-NEW");
    case ACTION_READ:
	return ("READ");
    case ACTION_WRITE:
	return ("WRITE");
    case ACTION_CLOSE:
	return ("CLOSE");
    case ACTION_SEEK:
	return ("SEEK");
    case ACTION_EXAMINE_NEXT:
	return ("EXAMINE NEXT");
    case ACTION_EXAMINE_OBJECT:
	return ("EXAMINE OBJ");
    case ACTION_INFO:
	return ("INFO");
    case ACTION_DISK_INFO:
	return ("DISK INFO");
    case ACTION_PARENT:
	return ("PARENTDIR");
    case ACTION_DELETE_OBJECT:
	return ("DELETE");
    case ACTION_CREATE_DIR:
	return ("CREATEDIR");
    case ACTION_LOCATE_OBJECT:
	return ("LOCK");
    case ACTION_COPY_DIR:
	return ("DUPLOCK");
    case ACTION_FREE_LOCK:
	return ("FREELOCK");
    case ACTION_SET_PROTECT:
	return ("SETPROTECT");
    case ACTION_SET_COMMENT:
	return ("SETCOMMENT");
    case ACTION_RENAME_OBJECT:
	return ("RENAME");
    case ACTION_INHIBIT:
	return ("INHIBIT");
    case ACTION_RENAME_DISK:
	return ("RENAME DISK");
    case ACTION_MORECACHE:
	return ("MORE CACHE");
    case ACTION_WAIT_CHAR:
	return ("WAIT FOR CHAR");
    case ACTION_FLUSH:
	return ("FLUSH");
    case ACTION_RAWMODE:
	return ("RAWMODE");
    case ACTION_SET_DATE:
	return ("SET_DATE");
    default:
	return ("---------UNKNOWN-------");
    }
}

#endif				/* HDEBUG */
