#include <stdio.h>

#include <layout.h>
#define MS_BPS		512

/*
 * Calculate the length between the sectors, given the length of the track
 * and the number of sectors that must fit on it.
 * The proper formula would be
 * (((TLEN/2) - INDEXLEN) / unit->mu_SectorsPerTrack) - BLOCKLEN;
 */

int
Gap(int tlen, int sectors)
{
    int gap = (((tlen/2) - INDEXLEN) / sectors) - BLOCKLEN;

    printf("%d %d -> %d\n", tlen, sectors, gap);

    return gap;
}

int
main()
{
    Gap(TLEN, 8);
    Gap(TLEN, 9);
    Gap(TLEN, 10);
    Gap(TLEN, 11);	/* won't work */

    Gap(TLEN*2, 15);
    Gap(TLEN*2, 16);
    Gap(TLEN*2, 17);
    Gap(TLEN*2, 18);
    Gap(TLEN*2, 19);
    Gap(TLEN*2, 20);
    Gap(TLEN*2, 21);
    Gap(TLEN*2, 22);	/* won't work */
}
