/*-
 *  $Id: dev.h,v 1.55 1993/12/30 22:45:10 Rhialto Rel $
 *  $Log: dev.h,v $
 * Revision 1.55  1993/12/30  22:45:10  Rhialto
 * Revamp flags and track size constants.
 * Freeze for MAXON5.
 *
 * Revision 1.54  1993/06/24  04:56:00	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:16:07  Rhialto
 * Add repeated #include protection.
 *
 * Revision 1.51  92/04/17  15:43:01  Rhialto
 * Freeze for MAXON3. Change cyl+side units to track units.
 *
 * Revision 1.46  91/10/06  18:26:48  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.42  91/06/14  00:07:09  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:56:29  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.30  90/06/04  23:19:21  Rhialto
 * Release 1 Patch 3
 *
 *  Include file for users of the messydisk.device
-*/
#ifndef MESSYDISK_DEV_H
#define MESSYDISK_DEV_H

#ifndef EXEC_TYPES_H
#include "exec/types.h"
#endif
#ifndef EXEC_MEMORY_H
#include "exec/memory.h"
#endif
#ifndef EXEC_SEMAPHORES_H
#include "exec/semaphores.h"
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
#ifndef EXEC_ERRORS_H
#include "exec/errors.h"
#endif
#ifndef EXEC_DEVICES_H
#include "exec/devices.h"
#endif
#ifndef DEVICES_TRACKDISK_H
#include "devices/trackdisk.h"
#endif

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long ulong;

#define IOMDB_40TRACKS	    7			/* in IOReqs and md_IOFlags */
#define IOMDF_40TRACKS	    (1<<7)

#define IOMDB_FIXFLAGS	    8			/* Fix md_IOFlags permanently */
#define IOMDF_FIXFLAGS	    (1<<8)

#define DiskResource	    DiscResource	/* Aargh! */
#define DiskResourceUnit    DiscResourceUnit	/* Aargh! */

/*
 * Some default values
 */

#define MS_BPS		512	/* Bytes per sector */
#define MS_NSIDES	2	/* Tracks per cylinder */

#define MS_SPT_DD	9	/* Default sectors per track for DD */
#define MS_SPT_MAX_DD	10	/* Max sectors per track for DD */
#define MS_SPT_HD	18	/* Default sectors per track for HD */
#define MS_SPT_MAX_HD	21	/* Max sectors per track for HD */

#define MS_SPT	    MS_SPT_DD	/* Default sectors per track */
#define MS_SPT_MAX  MS_SPT_MAX_HD /* Max sectors per track */

#endif	/* MESSYDISK_DEV_H */
