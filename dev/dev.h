/*-
 *  $Id: dev.h,v 1.2 90/01/27 20:38:18 Rhialto Rel $
 *
 *  Include file for users of the messydisk.device
-*/

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
#ifndef RESOURCES_DISK_H
#include "resources/disk.h"
#endif
#ifndef RESOURCES_CIA_H
#include "resources/cia.h"
#endif
#ifndef HARDWARE_CUSTOM_H
#include "hardware/custom.h"
#endif
#ifndef HARDWARE_CIA_H
#include "hardware/cia.h"
#endif
#ifndef HARDWARE_ADKBITS_H
#include "hardware/adkbits.h"
#endif
#ifndef HARDWARE_DMABITS_H
#include "hardware/dmabits.h"
#endif
#ifndef HARDWARE_INTBITS_H
#include "hardware/intbits.h"
#endif
#ifndef DEVICES_TRACKDISK_H
#include "devices/trackdisk.h"
#endif

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long ulong;

#define IOMDB_40TRACKS	7
#define IOMDF_40TRACKS	(1<<7)

#define DiskResource	    DiscResource	/* Aargh! */
#define DiskResourceUnit    DiscResourceUnit	/* Aargh! */

/*
 * Some default values
 */

#define MS_BPS	    512 	/* Bytes per sector */
#define MS_SPT	    9		/* Default sectors per track */
#define MS_SPT_MAX  10		/* Max sectors per track */
#define MS_NSIDES   2		/* Tracks per cylinder */
