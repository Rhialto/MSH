/*-
 * $Id$
 * $Log$
 *
 * May not be used or copied without a licence.
-*/

#include <amiga.h>
#include <functions.h>
#include "han.h"

#ifdef CONVERSIONS
#include "hanconv.h"

#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

void rdwr_CopyMem(byte *from, byte *to, long fromsize);
void rd_FromPC(byte *from, byte *to, long fromsize);
void rd_FromST(byte *from, byte *to, long fromsize);

void wr_ToPC(byte *from, byte *to, long fromsize);
void wr_ToST(byte *from, byte *to, long fromsize);

const ConversionFunc rd_Conv[] = {
    rdwr_CopyMem, rd_FromPC, rd_FromST
};

const ConversionFunc wr_Conv[] = {
    rdwr_CopyMem, wr_ToPC, wr_ToST
};

/*
 * The conversion table pointers can be shared among multiple running
 * instances of the handler, as long as we won't introduce user-defined
 * conversion tables. Then we can do a copy-on-write.
 * Must really protect against some race conditions with a shared semaphore.
 */

int		ConversionImbeddedInFileName;
static __shared unsigned char *Table_FromPC;
static __shared unsigned char *Table_FromST;
static __shared unsigned char *Table_ToPC;
static __shared unsigned char *Table_ToST;

void
rdwr_CopyMem(from, to, fromsize)
byte	       *from;
byte	       *to;
long		fromsize;
{
    CopyMem(from, to, fromsize);
}

void
rd_FromPC(from, to, fromsize)
byte	       *from;
byte	       *to;
long		fromsize;
{
    if ((Table_FromPC == NULL) && (Table_FromPC = AllocMem(256L, 0L))) {
	int i;

	for (i = 0; i < 256; i++)
	    Table_FromPC[i] = i;

	Table_FromPC[0x81] = 0xFC;   /* u */
	Table_FromPC[0x84] = 0xE4;   /* a */
	Table_FromPC[0x94] = 0xF6;   /* o */
	Table_FromPC[0x9A] = 0xDC;   /* U */
	Table_FromPC[0x8E] = 0xC4;   /* A */
	Table_FromPC[0x99] = 0xD6;   /* O */
	Table_FromPC[0x9E] = 0xDF;   /* ß */
    }

    if (Table_FromPC != NULL) {
	for (; fromsize > 0; fromsize--)
	    *to++ = Table_FromPC[*from++];
    }
}

void
rd_FromST(from, to, fromsize)
byte	       *from;
byte	       *to;
long		fromsize;
{
    if ((Table_FromST == NULL) && (Table_FromST = AllocMem(256L, 0L))) {
	int i;

	for (i = 0; i < 256; i++)
	    Table_FromST[i] = i;

	Table_FromST[0x81] = 0xFC;   /* u */
	Table_FromST[0x84] = 0xE4;   /* a */
	Table_FromST[0x94] = 0xF6;   /* o */
	Table_FromST[0x9A] = 0xDC;   /* U */
	Table_FromST[0x8E] = 0xC4;   /* A */
	Table_FromST[0x99] = 0xD6;   /* O */
	Table_FromST[0xE1] = 0xDF;   /* ß */
    }

    if (Table_FromST != NULL) {
	for (; fromsize > 0; fromsize--)
	    *to++ = Table_FromST[*from++];
    }
}

void
wr_ToPC(from, to, fromsize)
byte	       *from;
byte	       *to;
long		fromsize;
{
    if ((Table_ToPC == NULL) && (Table_ToPC = AllocMem(256L, 0L))) {
	int i;

	for (i = 0; i < 256; i++)
	    Table_ToPC[i] = i;

	Table_ToPC[0xFC] = 0x81;   /* u */
	Table_ToPC[0xE4] = 0x84;   /* a */
	Table_ToPC[0xF6] = 0x94;   /* o */
	Table_ToPC[0xDC] = 0x9A;   /* U */
	Table_ToPC[0xC4] = 0x8E;   /* A */
	Table_ToPC[0xD6] = 0x99;   /* O */
	Table_ToPC[0xDF] = 0x9E;   /* ß */
    }

    if (Table_ToPC != NULL) {
	for (; fromsize > 0; fromsize--)
	    *to++ = Table_ToPC[*from++];
    }
}

void
wr_ToST(from, to, fromsize)
byte	       *from;
byte	       *to;
long		fromsize;
{
    if ((Table_ToST == NULL) && (Table_ToST = AllocMem(256L, 0L))) {
	int i;

	for (i = 0; i < 256; i++)
	    Table_ToST[i] = i;

	Table_ToST[0xFC] = 0x81;   /* u */
	Table_ToST[0xE4] = 0x84;   /* a */
	Table_ToST[0xF6] = 0x94;   /* o */
	Table_ToST[0xDC] = 0x9A;   /* U */
	Table_ToST[0xC4] = 0x8E;   /* A */
	Table_ToST[0xD6] = 0x99;   /* O */
	Table_ToST[0xDF] = 0xE1;   /* ß */
    }

    if (Table_ToST != NULL) {
	for (; fromsize > 0; fromsize--)
	    *to++ = Table_ToST[*from++];
    }
}

void
ConvCleanUp()
{
    if (Table_FromPC) {
	FreeMem(Table_FromPC, 256L);
	Table_FromPC = NULL;
    }
    if (Table_FromST) {
	FreeMem(Table_FromST, 256L);
	Table_FromST = NULL;
    }
    if (Table_ToPC) {
	FreeMem(Table_ToPC, 256L);
	Table_ToPC = NULL;
    }
    if (Table_ToST) {
	FreeMem(Table_ToST, 256L);
	Table_ToST = NULL;
    }
}

#endif /* CONVERSIONS */
