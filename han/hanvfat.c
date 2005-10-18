/*-
 * $Id: hanvfat.c,v 1.58 2005/10/19 16:53:52 Rhialto Exp $
 * $Log: hanvfat.c,v $
 * Revision 1.58  2005/10/19  16:53:52  Rhialto
 * Finally a new version!
 *
 *
 * HANVFAT.C
 *
 * The code for the messydos file system handler
 *
 * The functions in this file all relate to the handling of long file
 * names according to the Losedows '95 system, also called VFAT.
 *
 * This code is (C) Copyright 1996-1997 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
 *
 * Much of this code is taken from or inspired by the NetBSD version
 * of the messy file system, and mtools. See msh.guide for credits.
-*/

#include <string.h>
#include "han.h"
#include "dos.h"

#if HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif
#define	debug0(x)

Prototype int CheckVfatSubentry(const byte *aminame, int amilength, const struct MsVfatSubEntry *se, int checksum);
Prototype int VfatChecksum(byte *name);
Prototype int ExamineVfatSubEntry(const struct MsVfatSubEntry *msd, struct FileInfoBlock *fib, int checksum);

#if VFATSUPPORT

/*
 * Table to convert uppercase letters to lowercase
 */
static byte
u2l[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 00-07 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, /* 08-0f */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 10-17 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, /* 18-1f */
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, /* 20-27 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, /* 28-2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 30-37 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, /* 38-3f */
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 40-47 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 48-4f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 50-57 */
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, /* 58-5f */
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 60-67 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, /* 68-6f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 70-77 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, /* 78-7f */
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, /* 80-87 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, /* 88-8f */
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, /* 90-97 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, /* 98-9f */
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, /* a0-a7 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, /* a8-af */
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, /* b0-b7 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, /* b8-bf */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* c0-c7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* c8-cf */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xd7, /* d0-d7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xdf, /* d8-df */
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, /* e0-e7 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, /* e8-ef */
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, /* f0-f7 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, /* f8-ff */
};

int
CheckVfatSubentry(
    const byte *aminame,
    int amilength,
    const struct MsVfatSubEntry *se,
    int checksum)
{
    int             i;
    const byte     *cp;

    debug0(("CheckVfatSubentry: se_Count = %x, se_Checksum = %d, checksum = %d\n",
	se->se_Count, se->se_Checksum, checksum));

    /* Compare checksums */
    if (se->se_Count & SE_LAST) {
	debug0(("CheckVfatSubentry: checksum = se->se_Checksum\n"));
	checksum = se->se_Checksum;
    } else if (checksum != se->se_Checksum) {
	debug0(("CheckVfatSubentry: checksum != se->se_Checksum\n"));
	checksum = -1;
    }
    if (checksum == -1) {
	debug0(("CheckVfatSubentry: returning checksum == -1\n"));
	return -1;
    }

    /*
     * Calculate offset of this entry. NetBSD's code assumed that
     * a 13-char file name would take one subentry, but mtools generates
     * 2 subentries for such a name. Therefore the test <= 0
     * has been changed to < 0, to tolerate mtools's behaviour.
     */
    i = ((se->se_Count & SE_COUNT) - 1) * SE_CHARS;
    if ((amilength -= i) < 0) {
	debug0(("CheckVfatSubentry: returning 'cause offset %d > %d\n",
		    i, amilength + i));
	return -1;
    }
    debug0(("CheckVfatSubentry: checking '%s' '%s'\n", aminame, aminame+i));
    aminame += i;

    for (cp = se->se_Part1, i = sizeof(se->se_Part1)/2; --i >= 0;) {
	if (--amilength < 0) {
	    if (!*cp++ && !*cp)
		return checksum;
	    return -1;
	}
	debug0(("Part1: %c =? %c\n", *cp, *aminame));
	if (u2l[*cp++] != u2l[*aminame++] || *cp++)
	    return -1;
    }
    for (cp = se->se_Part2, i = sizeof(se->se_Part2)/2; --i >= 0;) {
	if (--amilength < 0) {
	    if (!*cp++ && !*cp)
		return checksum;
	    return -1;
	}
	debug0(("Part2: %c =? %c\n", *cp, *aminame));
	if (u2l[*cp++] != u2l[*aminame++] || *cp++)
	    return -1;
    }
    for (cp = se->se_Part3, i = sizeof(se->se_Part3)/2; --i >= 0;) {
	if (--amilength < 0) {
	    if (!*cp++ && !*cp)
		return checksum;
	    return -1;
	}
	debug0(("Part3: %c =? %c\n", *cp, *aminame));
	if (u2l[*cp++] != u2l[*aminame++] || *cp++)
	    return -1;
    }
    
    return checksum;
}

int
VfatChecksum(byte *name)
{
    int i;
    byte s;

    for (s = 0, i = L_8 + L_3; --i >= 0; s += *name++)                   
	s = (s << 7)|(s >> 1);
    return s;                                                     
}

int
ExamineVfatSubEntry(
    const struct MsVfatSubEntry *se,
    struct FileInfoBlock *fib,
    int checksum)
{
    int i;
    byte           *np;
    byte           *cp;

    if ((se->se_Count & SE_COUNT) > SE_MAX_ENTRIES ||
	!(se->se_Count & SE_COUNT))
	return -1;
    
    /* Compare checksums */
    if (se->se_Count & SE_LAST) {
	/*
	 * Terminate the string in case it is an exact multiple
	 * of SE_CHARS characters. Assumes highest count is first.
	 */
	int length = (se->se_Count & SE_COUNT) * SE_CHARS;
	fib->fib_FileName[1 + length] = '\0';
	checksum = se->se_Checksum;
    } else if (checksum != se->se_Checksum)
	checksum = -1;
    if (checksum == -1)
	return -1;

    /* Offset of this entry */
    i = ((se->se_Count & SE_COUNT) - 1) * SE_CHARS;
    np = fib->fib_FileName + 1 + i;

    /* Convert the name parts */
    for (cp = se->se_Part1, i = sizeof(se->se_Part1) / 2; --i >= 0;) {
	switch (*np++ = *cp++) {
	case 0:
	    return checksum;
	case '/':
	    np[-1] = '\0';
	    return -1;
	}
	if (*cp++)
	    return -1;
    }
    for (cp = se->se_Part2, i = sizeof(se->se_Part2) / 2; --i >= 0;) {
	switch (*np++ = *cp++) {
	case 0:
	    return checksum;
	case '/':
	    np[-1] = '\0';
	    return -1;
	}
	if (*cp++)
	    return -1;
    }
    for (cp = se->se_Part3, i = sizeof(se->se_Part3) / 2; --i >= 0;) {
	switch (*np++ = *cp++) {
	case 0:
	    return checksum;
	case '/':
	    np[-1] = '\0';
	    return -1;
	}
	if (*cp++)
	    return -1;
    }
    return checksum;
}


static byte
ami2dos[256] = {
	0,    0,    0,    0,    0,    0,    0,    0,	/* 00-07 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 08-0f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 10-17 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 18-1f */
	0,    0x21, 0,    0x23, 0x24, 0x25, 0x26, 0,	/* 20-27 */
	0x28, 0x29, 0,    0,    0x2c, 0x2d, 0,    0,	/* 28-2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 30-37 */
	0x38, 0x39, 0,    0x3b, 0,    0,    0,    0,	/* 38-3f */
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 40-47 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,	/* 48-4f */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 50-57 */
	0x58, 0x59, 0x5a, 0,    0,    0,    0,    0x5f,	/* 58-5f */
	0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 60-67 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,	/* 68-6f */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 70-77 */
	0x58, 0x59, 0x5a, 0x7b, 0,    0x7d, 0x7e, 0,	/* 78-7f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 80-87 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 88-8f */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 90-97 */
	0,    0,    0,    0,    0,    0,    0,    0,	/* 98-9f */
	0,    0xad, 0xbd, 0x9c, 0xcf, 0xbe, 0xdd, 0xf5,	/* a0-a7 */
	0xf9, 0xb8, 0xa6, 0xae, 0xaa, 0xf0, 0xa9, 0xee,	/* a8-af */
	0xf8, 0xf1, 0xfd, 0xfc, 0xef, 0xe6, 0xf4, 0xfa,	/* b0-b7 */
	0xf7, 0xfb, 0xa7, 0xaf, 0xac, 0xab, 0xf3, 0xa8,	/* b8-bf */
	0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,	/* c0-c7 */
	0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,	/* c8-cf */
	0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0x9e,	/* d0-d7 */
	0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0xe1,	/* d8-df */
	0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,	/* e0-e7 */
	0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,	/* e8-ef */
	0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0xf6,	/* f0-f7 */
	0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0x98,	/* f8-ff */
};

static byte
dos2ami[256] = {
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,	/* 00-07 */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,	/* 08-0f */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,	/* 10-17 */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,	/* 18-1f */
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 20-27 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,	/* 28-2f */
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 30-37 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,	/* 38-3f */
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 40-47 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,	/* 48-4f */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 50-57 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,	/* 58-5f */
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 60-67 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,	/* 68-6f */
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 70-77 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,	/* 78-7f */
	0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,	/* 80-87 */
	0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,	/* 88-8f */
	0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,	/* 90-97 */
	0xff, 0xd6, 0xdc, 0xf8, 0xa3, 0xd8, 0xd7, 0x3f,	/* 98-9f */
	0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,	/* a0-a7 */
	0xbf, 0xae, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,	/* a8-af */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0xc1, 0xc2, 0xc0,	/* b0-b7 */
	0xa9, 0x3f, 0x3f, 0x3f, 0x3f, 0xa2, 0xa5, 0x3f,	/* b8-bf */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0xe3, 0xc3,	/* c0-c7 */
	0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0xa4,	/* c8-cf */
	0xf0, 0xd0, 0xca, 0xcb, 0xc8, 0x3f, 0xcd, 0xce,	/* d0-d7 */
	0xcf, 0x3f, 0x3f, 0x3f, 0x3f, 0xa6, 0xcc, 0x3f,	/* d8-df */
	0xd3, 0xdf, 0xd4, 0xd2, 0xf5, 0xd5, 0xb5, 0xfe,	/* e0-e7 */
	0xde, 0xda, 0xdb, 0xd9, 0xfd, 0xdd, 0xaf, 0x3f,	/* e8-ef */
	0xad, 0xb1, 0x3f, 0xbe, 0xb6, 0xa7, 0xf7, 0xb8,	/* f0-f7 */
	0xb0, 0xa8, 0xb7, 0xb9, 0xb3, 0xb2, 0x3f, 0x3f,	/* f8-ff */
};

Prototype int dos2amifn(byte dn[L_8 + L_3], byte *un, int lower);

/*
 * Convert a DOS filename to a amiga filename. And, return the number of
 * characters in the resulting amiga filename excluding the terminating
 * null.
 */
int
dos2amifn(
	byte dn[L_8 + L_3],
	byte *un,
	int lower)
{
	int i;
	int thislong = 1;
	byte c;

	/*
	 * If first char of the filename is DIR_E5_REPLACEMENT (0x05),
	 * then the real first char of the filename should be 0xe5. But,
	 * they couldn't just have a 0xe5 mean 0xe5 because that is used
	 * to mean a freed directory slot. Another dos quirk.
	 */
	if (*dn == DIR_E5_REPLACEMENT)
		c = dos2ami[0xe5];
	else
		c = dos2ami[*dn];
	*un++ = lower ? u2l[c] : c;
	dn++;
	
	/*
	 * Copy the name portion into the amiga filename string.
	 */
	for (i = 1; i < 8 && *dn != ' '; i++) {
		c = dos2ami[*dn++];
		*un++ = lower ? u2l[c] : c;
		thislong++;
	}
	dn += 8 - i;
	
	/*
	 * Now, if there is an extension then put in a period and copy in
	 * the extension.
	 */
	if (*dn != ' ') {
		*un++ = '.';
		thislong++;
		for (i = 0; i < 3 && *dn != ' '; i++) {
			c = dos2ami[*dn++];
			*un++ = lower ? u2l[c] : c;
			thislong++;
		}
	}
	*un++ = 0;

	return (thislong);
}

Prototype int ami2dosfn(byte *un, byte dn[L_8+L_3+1], int unlen, int gen);

/*
 * Convert a amiga filename to a DOS filename according to Win95 rules.
 * If applicable and gen is not 0, it is inserted into the converted
 * filename as a generation number.
 * Returns
 *	0 if name couldn't be converted
 *	1 if the converted name is the same as the original
 *	  (no long filename entry necessary for Win95)
 *	2 if conversion was successful
 *	3 if conversion was successful and generation number was inserted
 */
int
ami2dosfn(
	byte *un,
	byte dn[L_8+L_3+1],
	int unlen,
	int gen)
{
	int i, j, l;
	int conv = U2D_CONVERTED_SAME;
	byte *cp, *dp, *dp1;
	byte gentext[6];
	
	/*
	 * Fill the dos filename string with blanks. These are DOS's pad
	 * characters.
	 */
	for (i = 0; i < L_8 + L_3; i++)
		dn[i] = ' ';
	dn[L_8 + L_3] = 0;
	
	/*
	 * The filenames "." and ".." are handled specially, since they
	 * don't follow dos filename rules.
	 */
	if (un[0] == '.' && unlen == 1) {
		dn[0] = '.';
		return gen <= 1;
	}
	if (un[0] == '.' && un[1] == '.' && unlen == 2) {
		dn[0] = '.';
		dn[1] = '.';
		return gen <= 1;
	}

	/*
	 * Filenames with only blanks and dots are not allowed!
	 */
	for (cp = un, i = unlen; --i >= 0; cp++)
		if (*cp != ' ' && *cp != '.')
			break;
	if (i < 0)
		return U2D_CANNOT_CONVERT;
	
	/*
	 * Now find the extension
	 * Note: dot as first char doesn't start extension
	 *	 and trailing dots and blanks are ignored
	 */
	dp = dp1 = 0;
	for (cp = un + 1, i = unlen - 1; --i >= 0;) {
		switch (*cp++) {
		case '.':
			if (!dp1)
				dp1 = cp;
			break;
		case ' ':
			break;
		default:
			if (dp1)
				dp = dp1;
			dp1 = 0;
			break;
		}
	}
	
	/*
	 * Now convert it
	 */
	if (dp) {
		if (dp1)
			l = dp1 - dp;
		else
			l = unlen - (dp - un);
		for (i = 0, j = L_8; i < l && j < L_8 + L_3; i++, j++) {
			if (dp[i] != (dn[j] = ami2dos[dp[i]])
			    && conv != U2D_CONVERTED_GENNR)
				conv = U2D_CONVERTED_OK;
			if (!dn[j]) {
				conv = U2D_CONVERTED_GENNR;
				dn[j--] = ' ';
			}
		}
		if (i < l)
			conv = U2D_CONVERTED_GENNR;
		dp--;
	} else {
		for (dp = cp; *--dp == ' ' || *dp == '.';);
		dp++;
	}

	/*
	 * Now convert the rest of the name
	 */
	for (i = j = 0; un < dp && j < L_8; i++, j++, un++) {
		if (*un != (dn[j] = ami2dos[*un])
		    && conv != U2D_CONVERTED_GENNR)
			conv = U2D_CONVERTED_OK;
		if (!dn[j]) {
			conv = U2D_CONVERTED_GENNR;
			dn[j--] = ' ';
		}
	}
	if (un < dp)
		conv = U2D_CONVERTED_GENNR;
	/*
	 * If we didn't have any chars in filename,
	 * generate a default
	 */
	if (!j)
		dn[0] = '_';
	
	/*
	 * The first character cannot be E5,
	 * because that means a deleted entry
	 */
	if (dn[0] == 0xe5)
		dn[0] = DIR_E5_REPLACEMENT;
	
	/*
	 * If there wasn't any char dropped,
	 * there is no place for generation numbers
	 */
	if (conv != U2D_CONVERTED_GENNR) {
		if (gen > 1)
			return U2D_CANNOT_CONVERT;
		return conv;
	}
	
#if 1
	/* If no generation requested, just truncate filename */
	if (gen < 0)
		return U2D_CONVERTED_TRUNC;
#endif

	/*
	 * Now insert the generation number into the filename part
	 */
	for (cp = gentext + sizeof(gentext); cp > gentext && gen; gen /= 10)
		*--cp = gen % 10 + '0';
	if (gen)
		return U2D_CANNOT_CONVERT;
	for (i = L_8; dn[--i] == ' ';);
	if (gentext + sizeof(gentext) - cp + 1 > L_8 - i)
		i = L_8 - (gentext + sizeof(gentext) - cp + 1);
	dn[i++] = '~';
	while (cp < gentext + sizeof(gentext))
		dn[i++] = *cp++;		/* XXX i++ was i ? rhialto */
	return U2D_CONVERTED_GENNR;
}

/*
 * Create a Win95 long name directory entry
 * Note: assumes that the filename is valid,
 *	 i.e. doesn't consist solely of blanks and dots
 */
int
ami2winfn(
	byte *un,
	int unlen,
	struct MsVfatSubEntry *wep,
	int cnt,
	int chksum)
{
	byte *cp;
	int i;

	/*
	 * Drop trailing blanks and dots
	 */
	for (cp = un + unlen; *--cp == ' ' || *cp == '.'; unlen--);

	un += (cnt - 1) * SE_CHARS;
	unlen -= (cnt - 1) * SE_CHARS;
	
	/*
	 * Initialize MsVfatSubEntry to some useful default
	 */
	for (cp = (byte *)wep, i = sizeof(*wep); --i >= 0; *cp++ = 0xff);
	wep->se_Count = cnt;
	wep->se_Attributes = ATTR_WIN95;
	wep->se_Pad1 = 0;
	wep->se_Checksum = chksum;
	wep->se_Pad2 = 0;
	
	/*
	 * Now convert the filename parts
	 */
	for (cp = wep->se_Part1, i = sizeof(wep->se_Part1)/2; --i >= 0;) {
		if (--unlen < 0)
			goto done;
		*cp++ = *un++;
		*cp++ = 0;
	}
	for (cp = wep->se_Part2, i = sizeof(wep->se_Part2)/2; --i >= 0;) {
		if (--unlen < 0)
			goto done;
		*cp++ = *un++;
		*cp++ = 0;
	}
	for (cp = wep->se_Part3, i = sizeof(wep->se_Part3)/2; --i >= 0;) {
		if (--unlen < 0)
			goto done;
		*cp++ = *un++;
		*cp++ = 0;
	}
	if (!unlen)
		wep->se_Count |= SE_LAST;
	return unlen;

done:
	*cp++ = 0;
	*cp++ = 0;
	wep->se_Count |= SE_LAST;
	return 0;
}

Prototype int WriteLongName(struct DirEntry *de, int wincnt, byte *name, int componentlength);

/*
 * Write the long filename, in the location indicated by
 * de_VfatnameSector/Offset.
 *
 * Set the de_Sector/Offset to the next slot so the short name can be put
 * there. This happens even when the long name is not written!
 */
int
WriteLongName(struct DirEntry *de, int wincnt, byte *name, int componentlength)
{
    sector_t sector = de->de_VfatnameSector;
    int offset = de->de_VfatnameOffset;
    byte checksum = VfatChecksum(de->de_Msd.msd_Name);

    while (wincnt > 0) {
	byte *s = ReadSec(sector);
	struct MsVfatSubEntry *se = (struct MsVfatSubEntry *)(s + offset);

	ami2winfn(name, componentlength, se, wincnt, checksum);

	MarkSecDirty(s);
	FreeSec(s);

	NextDirEntry(&sector, &offset);
	wincnt--;
    }
    de->de_Sector = sector;
    de->de_Offset = offset;
}

Prototype int ToUniqueMSName(byte *component, struct MSFileLock *parentdir, byte *name, int longcomponentlength);

/*
 * Create a unique DOS name in component
 */
int
ToUniqueMSName(
    byte *component, 
    struct MSFileLock *parentdir, 
    byte *name, 
    int longcomponentlength)
{
    int error;
    int gen;

    debug(("ToUniqueMSName(%.11s, %08lx, %s, %d\n",
	    component, parentdir, name, longcomponentlength));

    for (gen = 1;; gen++) {
	sector_t secnr;

	/*
	 * Generate DOS name with generation number
	 */
	if (!ami2dosfn(name, component, longcomponentlength, gen))
	    return (gen == 1) ? ERROR_INVALID_COMPONENT_NAME :
				ERROR_OBJECT_EXISTS;

	/*
	 * Now look for a dir entry with this exact name
	 */
	secnr = DirClusterToSector(parentdir->msfl_Msd.msd_Cluster);

	debug(("ToUniqueMSName: gen = %d, cluster = %d\n",
		gen, parentdir->msfl_Msd.msd_Cluster));

	for (error = 0; !error; ) {
	    byte *sector;
	    struct MsDirEntry *dentp;

	    debug(("ToUniqueMSName: gen = %d, secnr = %d\n",
		    gen, secnr));

	    if (secnr == SEC_EOF)
		return 0;

	    if ((sector = ReadSec(secnr)) == NULL) {
		return ERROR_READ_PROTECTED;	/* XXX */
	    }
	    for (dentp = (struct MsDirEntry *)sector;
		    (char *)dentp < sector + Disk.bps;
		    dentp++) {
		if (dentp->msd_Name[0] == DIR_END) {
		    /*
		     * Last used entry and not found
		     */
		    FreeSec(sector);
		    return 0;
		}
		/*
		 * Ignore volume labels and Win95 entries
		 */
		if (dentp->msd_Attributes & ATTR_VOLUMELABEL)
		    continue;
		if (!memcmp(dentp->msd_Name, component, L_8+L_3)) {
		    error = ERROR_OBJECT_EXISTS;
		    break;
		}
	    }
	    FreeSec(sector);

	    secnr = NextDirSector(secnr);
	    debug(("ToUniqueMSName: next secnr = %d\n", secnr));
	}
    }
}

Prototype int winSlotCnt(byte *un, int unlen);

#define howmany(count, blocksize) (((count) + (blocksize) - 1) / (blocksize))

/*
 * Determine the number of slots necessary for Win95 names
 */
int
winSlotCnt(
	byte *un,
	int unlen)
{
	for (un += unlen; unlen > 0; unlen--)
		if (*--un != ' ' && *un != '.')
			break;
	if (unlen > SE_MAX_CHARS)
		return 0;
	return howmany(unlen, SE_CHARS);
}

Prototype void EraseLongName(struct MSFileLock *fl);

void
EraseLongName(struct MSFileLock *fl)
{
    sector_t	sector;
    int		offset;
    
    if (Interleave & OPT_NO_WIN95) {
	debug(("EraseLongName: OPT_NO_WIN95 set.\n"));
	return;
    }
    debug(("EraseLongName\n"));

    sector = fl->msfl_VfatnameSector;

    /* Try to erase forward from start of long name */
    if (sector) {
	offset = fl->msfl_VfatnameOffset;

	for (;;) {
	    byte *s;
	    struct MsDirEntry *msd;

	    s = ReadSec(sector);
	    if (!s)
		break;
	    msd = (struct MsDirEntry *)(s + offset);
	    if (msd->msd_Attributes != ATTR_WIN95) {
		FreeSec(s);
		break;
	    }
	    if (msd->msd_Name[0] != DIR_DELETED) {
		msd->msd_Name[0] = DIR_DELETED;
		MarkSecDirty(s);
	    }
	    FreeSec(s);
	    NextDirEntry(&sector, &offset);
	}

	sector = fl->msfl_VfatnameSector;
	offset = fl->msfl_VfatnameOffset;
    } else {
	sector = fl->msfl_DirSector;
	offset = fl->msfl_DirOffset;
    }

    /*
     * Try to erase backward from either long or short name.
     * This erases ALL subentries, whether they belong to this
     * shortname or not. If they don't, they were junk anyway.
     * */
    for (;;) {
	byte *s;
	struct MsDirEntry *msd;

	PrevDirEntry(&sector, &offset);
	if (sector == SEC_EOF)
	    break;
	s = ReadSec(sector);
	if (!s)
	    break;
	msd = (struct MsDirEntry *)(s + offset);
	if (msd->msd_Attributes != ATTR_WIN95) {
	    FreeSec(s);
	    break;
	}
	if (msd->msd_Name[0] != DIR_DELETED) {
	    msd->msd_Name[0] = DIR_DELETED;
	    MarkSecDirty(s);
	}
	FreeSec(s);
    }
}

/*
 * Clean out junk from the directory.
 * This is mainly left-over vfat subentries that have no
 * corresponding file anymore.
 *
 * This is done in back-to-front order, because clearing one entry
 * may cause the one preceeding it to be cleared as well.
 *
 * There is one case that should be handled front-to-back:
 * a sequence of subentries that belong together and to the following
 * shortname entry, but without SE_LAST flag in the frontmost subentry.
 *
 * The obvious solution is a recursive function. Because directories
 * with long names may grow very long, it does an explicit stack check.
 */

Prototype void CleanupDirectory(struct MSFileLock *dir);

/* Recursive, back-to-front solution */

void
CleanupDirectory(struct MSFileLock *dir)
{
    struct MsDirEntry nextmsd = { 0 };
    static struct MSFileLock *lastdir;

    if (Interleave & OPT_NO_WIN95) {
	debug(("EraseLongName: OPT_NO_WIN95 set.\n"));
	return;
    }

#if 0
    if (lastdir == dir)
	return;
    dir = lastdir;
#endif

    CleanupDirectoryRec(&nextmsd,
		DirClusterToSector(dir->msfl_Msd.msd_Cluster));
}

#if 0
static
int
stackleft(int needed)
{
    APTR bottom = (APTR)((byte *)&bottom - needed);

    debug(("tc_SPLower = %lx, bottom = %lx\n",
	    StackBottom, bottom));
    if (bottom < me->tc_SPLower)
	return 0;
    return 1;
}
#endif

Prototype int CleanupDirectoryRec(struct MsDirEntry *nextmsd, sector_t sector);

int
CleanupDirectoryRec(
    struct MsDirEntry *nextmsd,
    sector_t sector)
{
    int nextvalid = 0;

    /* Our own simple stack check */
    if ((byte *)&nextvalid - 200 < StackBottom) {
	/*
	 * 200: A (hopefully) conservative guess. Currently, each recursive
	 * call seems to take 52 bytes, but we need to allow some room for
	 * ReadSec() and other called functions.
	 */
	return 0;
    }

    {
	byte *s = ReadSec(sector);
	debug(("CleanupDirectoryRec: forwards processing sector %ld\n", sector));

	if (s) {
	    struct MsDirEntry *msd = (struct MsDirEntry *)s;
	    struct MsDirEntry *pmsd = nextmsd; 
	    int dirty = 0;
	    
	    for (; (byte *)msd < s + Disk.bps;
		    pmsd = msd, msd++, nextvalid = 1) {
		if (!nextvalid) {
		    /* do nothing */
		} else if (msd->msd_Name[0] != DIR_DELETED &&
		    msd->msd_Attributes == ATTR_WIN95) {
		    /* Various sanity checks:
		     * After a non-subentry, a subentry must have
		     * the SE_LAST flag set.
		     */
		    struct MsVfatSubEntry *se = (struct MsVfatSubEntry *)msd;

		    if ((pmsd->msd_Name[0] == DIR_DELETED ||
			    pmsd->msd_Attributes != ATTR_WIN95) &&
			!(se->se_Count & SE_LAST)) {
			msd->msd_Name[0] = DIR_DELETED;
			dirty++;
		    }
		}
	    }
	    *nextmsd = *pmsd;
	    if (dirty)
		MarkSecDirty(s);
	    FreeSec(s);
	}

	/*
	 * This can be optimised to recurse only when really needed,
	 * but that doesn't make it prettier.
	 */
	sector_t nextsector = NextDirSector(sector);
	if (nextsector == SEC_EOF) {
	    memset(nextmsd, 0, MS_DIRENTSIZE);
	    nextvalid = 1;
	} else {
	    nextvalid = CleanupDirectoryRec(nextmsd, nextsector);
	}
    }
    {
	byte *s = ReadSec(sector);
	debug(("CleanupDirectoryRec: backwards processing sector %ld\n", sector));

	if (s) {
	    struct MsDirEntry *msd = (struct MsDirEntry *)
		(s + Disk.bps - MS_DIRENTSIZE);
	    struct MsDirEntry *nmsd = nextmsd; 
	    int dirty = 0;
	    
	    for (; (byte *)msd >= s; nmsd = msd, msd--, nextvalid = 1) {
		if (!nextvalid) {
		    /* do nothing */
		} else if (nmsd->msd_Name[0] == DIR_END) {
		    /*
		     * If the next-to-last entry is a subentry, deleted or
		     * not, clear it out.
		     */
		    if (msd->msd_Attributes == ATTR_WIN95 ||
			msd->msd_Name[0] == DIR_DELETED) {
			msd->msd_Name[0] = DIR_END;
			dirty++;
		    }
		} else if (msd->msd_Name[0] != DIR_DELETED &&
			   msd->msd_Attributes == ATTR_WIN95) {
		    /* Various sanity checks:
		     * After a subentry there must be either
		     * 1 the short entry, if the count == 1, or
		     * 2 another subentry with no FIRST flag,
		     *   a count 1 less,
		     *   and the same checksum.
		     * Note that a deleted entry following a subentry
		     * should fail both these tests (count is incorrect).
		     */
		    struct MsVfatSubEntry *se = (struct MsVfatSubEntry *)msd;
		    struct MsVfatSubEntry *nse = (struct MsVfatSubEntry *)nmsd;

		    if ((se->se_Count & SE_COUNT) == 1 &&
			nmsd->msd_Attributes != ATTR_WIN95 &&
			se->se_Checksum == VfatChecksum(nmsd->msd_Name)) {
			/* ok - do nothing */
		    } else if (nmsd->msd_Attributes == ATTR_WIN95 &&
			    nse->se_Count == (se->se_Count & SE_COUNT) - 1 &&
			    nse->se_Checksum == se->se_Checksum) {
			/* ok - do nothing */
		    } else {
			msd->msd_Name[0] = DIR_DELETED;
			dirty++;
		    }
		}
	    }

	    *nextmsd = *nmsd;
	    if (dirty)
		MarkSecDirty(s);
	    FreeSec(s);
	}
    }
    return nextvalid;
}

#endif	/* VFATSUPPORT */
