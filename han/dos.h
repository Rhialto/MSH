
/*
 *  $Id: dos.h,v 1.1 89/12/17 20:06:01 Rhialto Exp Locker: Rhialto $
 */

#ifdef NOTDEF
#include "exec/types.h"
#include "exec/memory.h"
#include "libraries/dos.h"
#include "libraries/dosextens.h"
#include "libraries/filehandler.h"
#endif NOTDEF

/*
 *  ACTIONS which do not exist in dosextens.h but which indeed exist on
 *  the Amiga.
 */

#define ACTION_MORECACHE    18L
#define ACTION_FLUSH	    27L
#define ACTION_RAWMODE	    994L
#define ACTION_OPENRW	    1004L
#define ACTION_OPENOLD	    1005L
#define ACTION_OPENNEW	    1006L
#define ACTION_CLOSE	    1007L
#define ACTION_SEEK	    1008L

#define FIBB_HIDDEN 7L
#define FIBF_HIDDEN (1L<<FIBB_HIDDEN)

#define CTOB(x) (void *)(((long)(x))>>2)    /*  BCPL conversion */
#define BTOC(x) (void *)(((long)(x))<<2)

#define bmov(ss,dd,nn) CopyMem(ss,dd,(ulong)(nn))   /* Matt's habit */

#define DOS_FALSE   0L
#define DOS_TRUE    -1L

typedef struct Interrupt	INTERRUPT;
typedef struct Task		TASK;
typedef struct FileLock 	LOCK;
typedef struct FileInfoBlock	FIB;
typedef struct DosPacket	PACKET;
typedef struct Process		PROC;
typedef struct DeviceNode	DEVNODE;
typedef struct DeviceList	DEVLIST;
typedef struct DosInfo		DOSINFO;
typedef struct RootNode 	ROOTNODE;
typedef struct FileHandle	FH;
typedef struct MsgPort		PORT;
typedef struct Message		MSG;
typedef struct MinList		LIST;
typedef struct MinNode		NODE;
typedef struct DateStamp	STAMP;
typedef struct InfoData 	INFODATA;
typedef struct DosLibrary	DOSLIB;

#define PType (packet->dp_Type)
#define PArg1 (packet->dp_Arg1)
#define PArg2 (packet->dp_Arg2)
#define PArg3 (packet->dp_Arg3)
#define PArg4 (packet->dp_Arg4)
#define PRes1 (packet->dp_Res1)
#define PRes2 (packet->dp_Res2)

#define dl_MSFileLockList   dl_unused


/*
 *  (void *)  in C means 'pointer to anything'.  I use it
 *  extensively.
 */

extern void *AbsExecBase;

extern struct MsgPort *CreatePort();
extern void *AllocMem(), *RemHead(), *GetMsg();
extern void *FindTask(), *Open(), *OpenLibrary();

extern void   *dosalloc(), *NextNode(), *GetHead(), *GetTail();
extern void   btos(), returnpacket();

extern char *typetostr();

extern struct DeviceList *NewVolNode();
extern void FreeVolNode();
extern struct FileLock *NewFileLock();
extern long FreeFileLock();
extern int DiskRemoved();
extern void DiskInserted();
extern DEVLIST *WhichDiskInserted();
extern int CheckRead();
extern int CheckWrite();
