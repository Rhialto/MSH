/*-
 * $Id: date.c,v 1.52 92/09/06 00:22:03 Rhialto Exp $
 * $Log:	date.c,v $
 * Revision 1.52  92/09/06  00:22:03  Rhialto
 * Didn't believe in leap days and some other days.
 *
 * Revision 1.51  92/04/17  15:38:43  Rhialto
 * Freeze for MAXON3.
 *
 * Revision 1.50  92/02/12  21:24:58  Rhialto
 * New date-to-days function.
 *
 * Revision 1.46  91/10/06  18:27:01  Rhialto
 *
 * Freeze for MAXON
 *
 * Revision 1.42  91/06/14  00:09:48  Rhialto
 * DICE conversion
 *
 * Revision 1.40  91/03/03  17:55:18  Rhialto
 * Freeze for MAXON
 *
 * Revision 1.32  90/11/23  23:55:51  Rhialto
 * Prepare for syslog
 *
 * Revision 1.30  90/06/04  23:18:11  Rhialto
 * Release 1 Patch 3
 *
 * DATE.C
 *
 * Two date conversion routines: DateStamp <-> MSDOS date/time.
 *
 * This code is (C) Copyright 1989-1992 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
 */

#include <functions.h>
#include "han.h"
#ifndef LIBRARIES_DOS_H
#include <libraries/dos.h>
#endif

#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

Prototype void ToDateStamp(struct DateStamp *datestamp, word date, word time);
Prototype void ToMSDate(word *date, word *time, struct DateStamp *datestamp);

long unixdays(int year, int month, int day);
void YrMoDa(long intdat, long *yr, long *mo, long *da);

/*
Article 2344 of alt.sources.d:
Path: wn1.sci.kun.nl!sun4nl!mcsun!uunet!munnari.oz.au!yoyo.aarnet.edu.au!sirius.ucs.adelaide.edu.au!amodra
From: amodra@ucs.adelaide.edu.au (Alan Modra)
Newsgroups: alt.sources.d
Subject: Re: timelocal (struct tm to time_t conversion) algorithm...
Message-ID: <5699@sirius.ucs.adelaide.edu.au>
Date: 20 Dec 91 03:27:53 GMT
References: <20169@rpp386.cactus.org>
Organization: Information Technology Division, The University of Adelaide, AUSTRALIA
Lines: 48

From article <20169@rpp386.cactus.org>, by jfh@rpp386.cactus.org (John F Haugh II):
> While we are all trotting out our mktime.c de la dia, here is one that

Thought I'd trot out my contribution too !

The following routine uses a reasonably concise method to convert the
date, which is probably the most awkward part in generating GMT since
1/1/1970.  Feel free to tack this bit of code in whatever other sources
you like!

OIS: 2922 is original_unixdays(1978, 1, 1) so we can use it for
     AmigaDOS dates.

#include <stdlib.h>
#include <stdio.h>
*/


long unixdays(int year, int month, int day)
/* converts date to days since 1/1/1970
   no check is made for sensible parameters
   BTW this routine works for dates before 1/1/1970 too
   (The Gregorian calendar only goes back to 1582 though !)
*/
{
    if ( (month -= 3) < 0 ) /* 1..12 -> 10,11,0..9 */
    {			    /* Puts Feb last since it has 28 days */
	month += 12;	    /* Now days per month is 31,30,31,30,31, */
	year -= 1;	    /*	(starting from Mar)  31,30,31,30,31, */
    }			    /*			     31,28 */
#if 0	/* for those who like calculations ! */
    month *= 31+30+31+30+31;/* Note the pattern. */
    day += month/5;	    /* We make use of it here and in the next line */
    if ( month % 5 > 2 ) day += 1;
#else	/* for those who like table look-up ! */
    {
	static const int monthdays[12] = {
	    0, 31, 61, 92, 122, 153, 184, 214, 245, 275, 306, 337 };
	day += monthdays[month];
    }
#endif
    return (long)(year/4 - year/100 + year/400 + day) + year*365L - 719469L
	   - 2922;
}


#define BASEYEAR	    1978
#define DAYS_PER_YEAR	    365
#define HOURS_PER_DAY	    24
#define MINUTES_PER_HOUR    60
#define SECONDS_PER_MINUTE  60

#define DAYS_PER_WEEK	    7
#define MONTHS_PER_YEAR     12

#define MINUTES_PER_DAY     (MINUTES_PER_HOUR * HOURS_PER_DAY)
#define SECONDS_PER_DAY     ((long) SECONDS_PER_MINUTE * \
			     MINUTES_PER_HOUR * HOURS_PER_DAY)

#define LeapYear(year)  ((year & 3) == 0)   /* From 1-Mar-1901 to 28-Feb-2100 */

const int daycount[MONTHS_PER_YEAR] = {
	31,	28,    31,    30,    31,    30,
	31,	31,    30,    31,    30,    31
};

void
ToDateStamp(datestamp, date, time)
struct DateStamp *datestamp;
word date;
word time;
{
    {
	int hours, minutes, seconds;

	seconds = (time & 31) * 2;
	time >>= 5;
	minutes = time & 63;
	time >>= 6;
	hours = time;

	datestamp->ds_Minute = MINUTES_PER_HOUR * hours + minutes;
	datestamp->ds_Tick = TICKS_PER_SECOND * seconds;
    }

    {
	int		year, month, day;
	int		t;

	if (date < DATE_MIN)
	    date = DATE_MIN;

	day = date & 31;
	date >>= 5;
	month = (date & 15);
	date >>= 4;
	year = date + 1980;

	t = daycount[month - 1];
	if (month == 2 && LeapYear(year))
	    t++;
	if ((unsigned) month > 12 || (unsigned) day > t) {
	    day = 31;
	    month = 12;
	    year = 1979;
	}

#if 1
	datestamp->ds_Days = unixdays(year, month, day);
#else
	{
	    ulong i, j, t;
	    j = year - BASEYEAR;

	    /* Get the next lower full leap period (4 years and a day) since ... */
	    t = (year - BASEYEAR) & ~3;
	    i = t;
	    t = (t / 4) * (4 * DAYS_PER_YEAR + 1);

	    /* t now is the number of days in 4 whole years since ... */

	    while (i < j) {
		t += DAYS_PER_YEAR;
		if (LeapYear(i + BASEYEAR)) {
		    t++;
		}
		i++;
	    }

	    /* t now is the number of days in whole years since ... */

	    month--;	/* 1..12 -> 0..11 */
	    for (i = 0; i < month; i++) {
		t += daycount[i];
		if (i == 1 && LeapYear(year)) {
		    t++;
		}
	    }

	    /* t now is the number of days in whole months since ... */

	    t += day - 1;

	    /* t now is the number of days in whole days since ... */

	    datestamp->ds_Days = t;
	}
#endif
    }
}

void YrMoDa(long intdat, long *yr, long *mo, long *da);

/*
Article 11708 of comp.sys.amiga.programmer:
Path: wn1.sci.kun.nl!sun4nl!mcsun!uunet!elroy.jpl.nasa.gov!decwrl!public!thad
From: thad@public.BTR.COM (Thaddeus P. Floryan)
Newsgroups: comp.sys.amiga.programmer
Subject: Re: Date Conversion..
Message-ID: <5396@public.BTR.COM>
Date: 2 Feb 92 07:43:43 GMT
References: <3125@seti.UUCP>
Organization: BTR Public Access UNIX, Mountain View CA
Lines: 130

In article <3125@seti.UUCP> oudejans@bora.inria.fr (Jeroen Oudejans) writes:
>
>Can anybody post me some code which recalculates a 'human readable' date
>from seconds after 1978 ?
>Or is there a library available (i am not using OS2, so can't use Amiga2Date())

Enclosed is something I last posted in 1988.  A more-commented version was to
appear in one of Rob Peck's books but for his untimely death July 2, 1990.

Thad Floryan [ thad@btr.com (OR) {decwrl, mips, fernwood}!btr!thad ]

-------------------- begin enclosure

From: thad@cup.portal.com
Newsgroups: comp.sys.amiga.tech
Subject: Re: Decoding a DateStamp
Message-ID: <5276@cup.portal.com>
Date: 10 May 88 09:20:41 GMT

The following code may be useful to you.  It's one of several hundreds of
kwik'n'dirty throwaways I did back in '85 when testing every documented
feature of the Amiga.  The "basic" code also works fine on DEC-20, Vax,
AT&T UNIX PC, C64/C128, and every other system I've tried it on.

Originally compiled this with Lattice 3.02; to compile with any recent Manx:

	CLI> cc +L dater
	CLI> ln dater -lc32

Feel welcome to use this example for your own learning; I'm intending this
(along with my complete date arithmetic package) to be part of an article
I intend completing (Real Soon Now :-) along with examples in C, assembler,
Fortran, AmigaBasiC, Modula-2 and Pascal.  This specific example has worked
perfectly thru AmigaDOS 1.0, 1.1, and 1.2 (and presumably 1.3 and beyond).

----------cut 'ere----------
*/
#if 0
/* DATER.C     Check out the DateStamp AmigaDOS system function
 *
 *    see page 2-15 in the AmigaDOS Developer's Manual for more info
 */

struct DS {	     /* DateStamp structure */
   long  NDays;      /* Days from Jan. 1, 1978 (a Sunday) */
   long  NMinutes;   /* Minutes into the current day */
   long  NTicks;     /* Clock ticks (1/50 sec = 1 tick) in current second */
};

static char *wkdays[] =
{"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

static char *mthnam[] =
{"","JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};

main()
{
   void DateStamp(), YrMoDa();

   struct DS datime;
   long month, day, year, hour, minute, second;

   DateStamp(&datime);
   printf("Days since 1-JAN-78 = %d\n", datime.NDays);
   printf("Minutes into today  = %d\n", datime.NMinutes);
   printf("1/50 secs in minute = %d\n", datime.NTicks);

   YrMoDa(datime.NDays, &year, &month, &day);

   printf("\nTherefore today is ");
   printf("%s ", wkdays[datime.NDays % 7]);

   printf("%d-%s-%d", day, mthnam[month], year);

   hour   = datime.NMinutes / 60;
   minute = datime.NMinutes % 60;
   second = datime.NTicks / 50;
   printf(" %2d:%02d:%02d\n", hour, minute, second);

   exit(0);
}
#endif
/****************************** YrMoDa **********************************
 *
 *    Extracts the component month, day and year from an AmigaDOS
 *    internal `day number' based from Jan.1, 1978.
 *
 *    The calculations herein use the following assertions:
 *
 *    146097 = number of days in 400 years per 400 * 365.2425 = 146097.00
 *     36524 = number of days in 100 years per 100 * 365.2425 =  36524.25
 *	1461 = number of days in   4 years per	 4 * 365.2425 =   1460.97
 *
 *    Thad Floryan, 12-NOV-85
 */

#define DDELTA 722449	/* days from Jan.1,0000 to Jan.1,1978 */

static int mthvec[] =
   {-1, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364};

void YrMoDa(intdat, yr, mo, da)
   long intdat;   /* I: AmigaDOS format, days since 1-JAN-78 */
   long *yr;	  /* O: resultant year in form YYYY */
   long *mo;	  /* O: resultant month in form MM */
   long *da;	  /* O: resultant day of month in form DD */
{
   register long jdate, day0, day1, day2, day3;

   jdate = intdat + DDELTA;  /* adjust internal date to Julian */

   *yr	 = (jdate / 146097) * 400;
   day0  = day1 = jdate %= 146097;
   *yr	+= (jdate / 36524) * 100;
   day2  = day1 %= 36524;
   *yr	+= (day2 / 1461) * 4;
   day3  = day1 %= 1461;
   *yr	+= day3 / 365;
   *mo	 = 1 + (day1 %= 365);
   *da	 = *mo % 30;
   *mo	/= 30;

   if ( ( day3 >= 59 && day0 < 59 ) ||
	( day3 <  59 && (day2 >= 59 || day0 < 59) ) )
      ++day1;

   if (day1 > mthvec[1 + *mo]) ++*mo;
   *da = day1 - mthvec[*mo];
}
/* end of DATER.C */

/*-------------------- end enclosure */

void
ToMSDate(date, time, datestamp)
word *date;
word *time;
struct DateStamp *datestamp;
{
    {
	word hours, minutes, seconds;

	hours = datestamp->ds_Minute / MINUTES_PER_HOUR;
	minutes = datestamp->ds_Minute % MINUTES_PER_HOUR;
	seconds = datestamp->ds_Tick / TICKS_PER_SECOND;

	*time = (hours << 11) | (minutes << 5) | (seconds / 2);
    }
#if 1
    {
	long year, month, day;

	YrMoDa(datestamp->ds_Days, &year, &month, &day);
	*date = ((year - 1980) << 9) | (month << 5) | day;
    }
#else
    {
	register long days, i, t;
	int year, month, day;

	days = datestamp->ds_Days;

	year = BASEYEAR + (days/(4*DAYS_PER_YEAR+1)) * 4;
	days %= 4 * DAYS_PER_YEAR + 1;
	while (days) {
		t = DAYS_PER_YEAR;
		if (LeapYear(year))
			t++;
		if (days < t)
			break;
		days -= t;
		year++;
	}
	days++;
	for (i = 0; i < MONTHS_PER_YEAR; i++) {
		t = daycount[i];
		if (i == 1 && LeapYear(year))
			t++;
		if (days <= t)
			break;
		days -= t;
	}
	month = i + 1;
	day = days;

	*date = ((year - 1980) << 9) | (month << 5) | day;
    }
#endif
}
