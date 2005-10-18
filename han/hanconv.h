/*-
 * $Id: hanconv.h,v 1.58 2005/10/19 16:53:52 Rhialto Exp $
 * $Log: hanconv.h,v $
 * Revision 1.58  2005/10/19  16:53:52  Rhialto
 * Finally a new version!
 *
 * Revision 1.56  1996/12/22  00:22:33  Rhialto
 * Cosmetics.
 *
 * Revision 1.55  1993/12/30  23:28:00  Rhialto
 * Freeze for MAXON5.
 *
 * Revision 1.54  1993/06/24  05:12:49  Rhialto
 * DICE 2.07.54R.
 *
 * Revision 1.53  92/10/25  03:04:02  Rhialto
 * Moved prototypes out of header file.
 * 
 * Revision 1.52  92/09/06  00:10:25  Rhialto
 * Clean up declaration.
 *
 * Revision 1.46  91/10/06  18:25:14  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.45  91/10/03  23:35:59  Rhialto
 * Initial version
 *
 * HANCONV.H
 *
 * This code is (C) Copyright 1991-1992 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#if CONVERSIONS

typedef void (*ConversionFunc)(byte *from, byte *to, long fromsize);
extern const ConversionFunc rd_Conv[];
extern const ConversionFunc wr_Conv[];

enum Conversion {
    ConvNone, ConvPCASCII, ConvSTASCII,
    ConvFence
};

#endif /* CONVERSIONS */
