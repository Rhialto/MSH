/*
 * $Id$
 * $Log$
 * MKCI.C
 *
 * Generate c.i file for assembly parts to export stuff from C to ASM level.
 */

#include <amiga.h>
#include "dev.h"
#include "device.h"
#include <stdio.h>

#define OFFSETOF(tag, member)   ((long)(&((struct tag *)0)->member))
#define OFFSET(label, structure, member) \
	printf("%s\tEQU %d\n", label, OFFSETOF(structure, member))
#define VALUE(label, value) \
	printf("%s\tEQU %d\n", label, value);

long
log2(unsigned long x)
{
    long log;

    if (x) {
	for (log = -1; x; log++)
	    x >>= 1;

	return log;
    } else {
	return -1;
    }
}

main(argc, argv)
int argc;
char **argv;
{
    if (argc > 1)
	freopen(argv[1], "w", stdout);

    OFFSET("md_Rawbuffer", MessyDevice, md_Rawbuffer);
    OFFSET("md_MfmDecode", MessyDevice, md_MfmDecode[0]);

    OFFSET("mu_TrackBuffer", MessyUnit, mu_TrackBuffer[0]);
    OFFSET("mu_CrcBuffer", MessyUnit, mu_CrcBuffer[0]);
    OFFSET("mu_SectorStatus", MessyUnit, mu_SectorStatus[0]);
    OFFSET("mu_InitSectorStatus", MessyUnit, mu_InitSectorStatus);
    OFFSET("mu_CurrentCylinder", MessyUnit, mu_CurrentCylinder);
    OFFSET("mu_CurrentSide", MessyUnit, mu_CurrentSide);
    OFFSET("mu_CurrentSectors", MessyUnit, mu_CurrentSectors);

#ifdef READONLY
    VALUE("READONLY", 1);
#else
    VALUE("READONLY", 0);
#endif
    VALUE("MS_BPS", MS_BPS);
    VALUE("MS_BPScode", log2(MS_BPS / 128));
    VALUE("LOG2_MS_BPS", log2(MS_BPS));
    VALUE("MS_SPT", MS_SPT);
    VALUE("MS_SPT_MAX", MS_SPT_MAX);

    return 0;
}

