/*-
 * $Id: hanconv.h,v 1.46 91/10/06 18:25:14 Rhialto Rel $
 * $Log:	hanconv.h,v $
 * Revision 1.46  91/10/06  18:25:14  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.45  91/10/03  23:35:59  Rhialto
 * Initial version
 *
 *
 * HANCONV.H
 *
 * This code is (C) Copyright 1991-1992 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#ifdef CONVERSIONS

typedef void (*ConversionFunc)(byte *from, byte *to, long fromsize);
extern const ConversionFunc rd_Conv[];
extern const ConversionFunc wr_Conv[];

enum Conversion {
    ConvUnspecified = -1,
    ConvNone, ConvPCASCII, ConvSTASCII,
    ConvFence
};

extern int	ConversionImbeddedInFileName;
void		ConvCleanUp(void);

#endif /* CONVERSIONS */
