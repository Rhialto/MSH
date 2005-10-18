
/*
 *  $Id: dos.h,v 1.58 2005/10/19 16:53:52 Rhialto Exp $
 *  $Log: dos.h,v $
 * Revision 1.58  2005/10/19  16:53:52  Rhialto
 * Finally a new version!
 *
 * Revision 1.56  1996/12/22  00:22:33  Rhialto
 * Remove old packet names and add some new (Guru Book) ones.
 *
 * Revision 1.55  1993/12/30  23:28:00	Rhialto
 * Freeze for MAXON5.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:44:02  Rhialto
 * No real change.
 *
 * Revision 1.52  92/09/06  00:23:30  Rhialto
 * Didn't believe in leap days and some other days.
 *
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

#ifndef CLIB_DOS_PROTOS_H
#include <clib/dos_protos.h>
#endif

extern struct DosLibrary *DOSBase;

/*
 *  ACTIONS which do not exist in dosextens.h but which indeed exist on
 *  the Amiga.
 */

#define ACTION_MORECACHE    18L
#if !defined(ACTION_FLUSH)
#define ACTION_FLUSH	    27L
#endif
#if !defined(ACTION_SEEK)
#define ACTION_SEEK	    1008L
#endif
#if !defined(ACTION_DIRECT_READ)
#define ACTION_DIRECT_READ  1900L
#endif
#if !defined(ACTION_GET_DISK_FSSM)
#define ACTION_GET_DISK_FSSM	4201L
#define ACTION_FREE_DISK_FSSM	4202L
#endif

#if H_BIT_MEANS_HIDDEN && !defined(FIBB_HIDDEN)
#define FIBB_HIDDEN 7L
#define FIBF_HIDDEN (1L<<FIBB_HIDDEN)
#endif

#if !defined(DE_DOSTYPE)
#define DE_DOSTYPE	    16L
#endif

#define CTOB(x) 	(((long)(x))>>2)    /*	BCPL conversion */
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
