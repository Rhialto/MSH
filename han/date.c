/*-
 * $Id: date.c,v 1.46 91/10/06 18:27:01 Rhialto Rel $
 * $Log:	date.c,v $
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
 * This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 * not be used or copied without a licence.
 */

#include <amiga.h>
#include <functions.h>
#ifndef LIBRARIES_DOS_H
#include <libraries/dos.h>
#endif
#include "han.h"

#ifdef HDEBUG
#   include "syslog.h"
#else
#   define	debug(x)
#endif

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
	static int monthdays[12] = {
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

int daycount[MONTHS_PER_YEAR] = {
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
	int year, month, day;

	if (date < DATE_MIN)
	    date = DATE_MIN;

	day = date & 31;
	date >>= 5;
	month = (date & 15);
	date >>= 4;
	year = date + 1980;

	if ((unsigned)month > 12 ||
	    (unsigned)day > (unsigned)daycount[month]) {
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

void
ToMSDate(date, time, datestamp)
word *date;
word *time;
register struct DateStamp *datestamp;
{
    {
	word hours, minutes, seconds;

	hours = datestamp->ds_Minute / MINUTES_PER_HOUR;
	minutes = datestamp->ds_Minute % MINUTES_PER_HOUR;
	seconds = datestamp->ds_Tick / TICKS_PER_SECOND;

	*time = (hours << 11) | (minutes << 5) | (seconds / 2);
    }
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
}
