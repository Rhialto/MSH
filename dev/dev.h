/*-
 *  $Id: dev.h,v 1.1 89/12/17 20:07:52 Rhialto Exp Locker: Rhialto $
 *
 *  Include file for users of the messydisk.device
-*/

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
