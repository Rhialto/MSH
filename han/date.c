/*-
 * $Id: date.c,v 1.42 91/06/14 00:09:48 Rhialto Exp $
 * $Log:	date.c,v $
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
#   define	debug(x)  syslog x
#else
#   define	debug(x)
#endif

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
	ulong i, j, t;
	int year, month, day;

	if (date < DATE_MIN)
	    date = DATE_MIN;

	day = date & 31;
	date >>= 5;
	month = (date & 15) - 1;
	date >>= 4;
	year = date + 1980;

	if ((unsigned)month > 11 ||
	    (unsigned)day > (unsigned)daycount[month]) {
	    day = 31;
	    month = 11;
	    year = 1979;
	}

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
