
/*
 *  $Id: dos.h,v 1.51 92/04/17 15:38:54 Rhialto Rel $
 *  $Log:	dos.h,v $
 * Revision 1.51  92/04/17  15:38:54  Rhialto
 * Freeze for MAXON3.
 *
 * Revision 1.46  91/10/06  18:26:32  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.42  91/06/14  00:05:47  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:55:29  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.30  90/06/04  23:18:20  Rhialto
 * Release 1 Patch 3
 *
 */

#ifndef EXEC_TYPES_H
#include "exec/types.h"
#endif
#ifndef EXEC_MEMORY_H
#include "exec/memory.h"
#endif
#ifndef EXEC_INTERRUPTS_H
#include "exec/interrupts.h"
#endif
#ifndef EXEC_NODES_H
#include "exec/nodes.h"
#endif
#ifndef EXEC_PORTS_H
#include "exec/ports.h"
#endif
#ifndef EXEC_IO_H
#include "exec/io.h"
#endif
#ifndef LIBRARIES_DOS_H
#include "libraries/dos.h"
#endif
#ifndef LIBRARIES_DOSEXTENS_H
#include "libraries/dosextens.h"
#endif
#ifndef LIBRARIES_FILEHANDLER_H
#include "libraries/filehandler.h"
#endif
#ifndef DEVICES_TRACKDISK_H
#include "devices/trackdisk.h"
#endif
#ifndef DEVICES_TIMER_H
#include "devices/timer.h"
#endif

/*
 *  ACTIONS which do not exist in dosextens.h but which indeed exist on
 *  the Amiga.
 */

#define ACTION_MORECACHE    18L
#ifndef ACTION_FLUSH
#define ACTION_FLUSH	    27L
#endif
#define ACTION_RAWMODE	    994L
#define ACTION_OPENRW	    1004L
#define ACTION_OPENOLD	    1005L
#define ACTION_OPENNEW	    1006L
#define ACTION_CLOSE	    1007L
#ifndef ACTION_SEEK
#define ACTION_SEEK	    1008L
#endif

#ifndef FIBB_HIDDEN
#define FIBB_HIDDEN 7L
#define FIBF_HIDDEN (1L<<FIBB_HIDDEN)
#endif

#ifndef DE_DOSTYPE
#define DE_DOSTYPE	    16L
#endif

#ifndef ID_MSDOS_DISK
#define ID_MSDOS_DISK	    0x4D534400L     /* 'MSD\0' */
#endif

#define CTOB(x)         (((long)(x))>>2)    /*  BCPL conversion */
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
