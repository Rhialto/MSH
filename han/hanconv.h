/*-
 * $Id$
 * $Log$
 *
 * HANCONV.H
 *
 * This code is (C) Copyright 1991 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#ifdef CONVERSIONS

typedef void (*ConversionFunc)(byte *from, byte *to, long fromsize);
extern ConversionFunc rd_Conv[];
extern ConversionFunc wr_Conv[];

enum Conversion {
    ConvNone, ConvPCASCII, ConvSTASCII,
    ConvFence
};

extern int	ConversionImbeddedInFileName;
void		ConvCleanUp(void);

#endif /* CONVERSIONS */
