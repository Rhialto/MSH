/*-
 * $Id: hanconv.c,v 1.55 1993/12/30 23:28:00 Rhialto Rel $
 * $Log: hanconv.c,v $
 * Revision 1.55  1993/12/30  23:28:00	Rhialto
 * Freeze for MAXON5.
 *
 * Revision 1.54  1993/06/24  05:12:49	Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  02:41:38  Rhialto
 * Rearrange initialisation of tables.
 *
 * Revision 1.51  92/04/17  15:38:09  Rhialto
 * Freeze for MAXON.
 *
 * Revision 1.48  91/11/03  00:56:34  Rhialto
 * Codes for s on PC/ST were swapped.
 *
 * Revision 1.46  91/10/06  18:24:51  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.45  91/10/03  23:34:41  Rhialto
 * Initial version
 *
 *
 * HANCONV.C
 *
 * The code for the messydos file system handler.
 *
 * This parts handles conversions on file data: PC/ST codes vs ISO-Latin-1.
 *
 * This code is (C) Copyright 1991-1993 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include "han.h"

#if CONVERSIONS
#include "hanconv.h"

#if HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

unsigned char  *InitTable(unsigned char *init);

void		rdwr_CopyMem(byte *from, byte *to, long fromsize);
void		rd_FromPC(byte *from, byte *to, long fromsize);
void		rd_FromST(byte *from, byte *to, long fromsize);

void		wr_ToPC(byte *from, byte *to, long fromsize);
void		wr_ToST(byte *from, byte *to, long fromsize);

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

Prototype short 	    ConversionImbeddedInFileName;
Prototype short 	    DefaultConversion;
Prototype __shared unsigned char *Table_ToPC;
Prototype __shared unsigned char *Table_FromPC;
Prototype __shared unsigned char *Table_ToST;
Prototype __shared unsigned char *Table_FromST;
Prototype void		  ConvCleanUp(void);

short		  ConversionImbeddedInFileName;
short		  DefaultConversion = ConvNone;
__shared unsigned char *Table_ToPC;
__shared unsigned char *Table_FromPC;
__shared unsigned char *Table_ToST;
__shared unsigned char *Table_FromST;
void		ConvCleanUp(void);

unsigned char  *
InitTable(init)
unsigned char  *init;
{
    unsigned char  *table;

    if (table = AllocMem(256L, 0L)) {
	int		i;

	for (i = 0; i < 256; i++)
	    table[i] = i;

	do {
	    i = *init++;
	    table[i] = *init++;
	} while (i != 0);
    }

    return table;
}

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
    static unsigned char init_FromPC[] = {
	0x81, 0xFC,   /* u */
	0x84, 0xE4,   /* a */
	0x94, 0xF6,   /* o */
	0x9A, 0xDC,   /* U */
	0x8E, 0xC4,   /* A */
	0x99, 0xD6,   /* O */
	0xE1, 0xDF,   /* s */
	0, 0
    };

    Forbid();
    if (Table_FromPC == 0)
	Table_FromPC = InitTable(init_FromPC);
    Permit();

    if (Table_FromPC != 0) {
	unsigned char *table = Table_FromPC;

	for (; fromsize > 0; fromsize--, from++, to++)
	    *to = table[*from];
    }
}

void
rd_FromST(from, to, fromsize)
byte	       *from;
byte	       *to;
long		fromsize;
{
    static unsigned char init_FromST[] = {
	0x81, 0xFC,   /* u */
	0x84, 0xE4,   /* a */
	0x94, 0xF6,   /* o */
	0x9A, 0xDC,   /* U */
	0x8E, 0xC4,   /* A */
	0x99, 0xD6,   /* O */
	0x9E, 0xDF,   /* s */
	0, 0
    };

    Forbid();
    if (Table_FromST == 0)
	Table_FromST = InitTable(init_FromST);
    Permit();

    if (Table_FromST != 0) {
	unsigned char *table = Table_FromST;

	for (; fromsize > 0; fromsize--, from++, to++)
	    *to = table[*from];
    }
}

void
wr_ToPC(from, to, fromsize)
byte	       *from;
byte	       *to;
long		fromsize;
{
    static unsigned char init_ToPC[] = {
	0xFC, 0x81,   /* u */
	0xE4, 0x84,   /* a */
	0xF6, 0x94,   /* o */
	0xDC, 0x9A,   /* U */
	0xC4, 0x8E,   /* A */
	0xD6, 0x99,   /* O */
	0xDF, 0xE1,   /* s */
	0, 0
    };

    Forbid();
    if (Table_ToPC == 0)
	Table_ToPC = InitTable(init_ToPC);
    Permit();

    if (Table_ToPC != 0) {
	unsigned char *table = Table_ToPC;

	for (; fromsize > 0; fromsize--, from++, to++)
	    *to = table[*from];
    }
}

void
wr_ToST(from, to, fromsize)
byte	       *from;
byte	       *to;
long		fromsize;
{
    static unsigned char init_ToST[] = {
	0xFC, 0x81,   /* u */
	0xE4, 0x84,   /* a */
	0xF6, 0x94,   /* o */
	0xDC, 0x9A,   /* U */
	0xC4, 0x8E,   /* A */
	0xD6, 0x99,   /* O */
	0xDF, 0x9E,   /* s */
	0, 0
    };

    Forbid();
    if (Table_ToST == 0)
	Table_ToST = InitTable(init_ToST);
    Permit();

    if (Table_ToST != 0) {
	unsigned char *table = Table_ToST;

	for (; fromsize > 0; fromsize--, from++, to++)
	    *to = table[*from];
    }
}

void
ConvCleanUp()
{
    Forbid();
    if (Table_FromPC) {
	FreeMem(Table_FromPC, 256L);
	Table_FromPC = 0;
    }
    if (Table_FromST) {
	FreeMem(Table_FromST, 256L);
	Table_FromST = 0;
    }
    if (Table_ToPC) {
	FreeMem(Table_ToPC, 256L);
	Table_ToPC = 0;
    }
    if (Table_ToST) {
	FreeMem(Table_ToST, 256L);
	Table_ToST = 0;
    }
    Permit();
}

#endif /* CONVERSIONS */
