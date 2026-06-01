#define _GNU_SOURCE
#include "time.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

time_t mxrec_now(void)
{
	static time_t now_time = -1;
	if (now_time == -1)
		now_time = time(NULL);
	return now_time;
}

time_t string2timestamp(const char *str)
{
	if (!str)
		return (time_t)-1;

	while (isspace((unsigned char)*str))
		str++;

	if (*str == '\0')
		return (time_t)-1;

	{
		struct tm tm = {0};
		char *rest;

		rest = strptime(str, "%Y-%m-%d %H:%M:%S", &tm);
		if (rest == NULL)
			rest = strptime(str, "%Y-%m-%dT%H:%M:%S", &tm);
		if (rest == NULL)
			rest = strptime(str, "%Y-%m-%d", &tm);

		if (rest != NULL) {
			if (*rest == '.') {
				while (isdigit((unsigned char)*rest))
					rest++;
			}

			while (isspace((unsigned char)*rest))
				rest++;
			if (*rest == '\0') {
				tm.tm_isdst = -1;
				time_t t = mktime(&tm);
				if (t != (time_t)-1)
					return t;
			}
		}
	}

	{
		char *end;
		long value = strtol(str, &end, 10);
		if (str == end || value < 0)
			return (time_t)-1;

		while (isspace((unsigned char)*end))
			end++;

		const char *unit_start = end;
		while (*end && !isspace((unsigned char)*end))
			end++;
		size_t uint_len = end - unit_start;
		while (isspace((unsigned char)*end))
			end++;
		if (*end != '\0')
			return (time_t)-1;

		unsigned long multiplier;
		if (uint_len == 0) {
			multiplier = 1;
		} else if (strncmp(unit_start, "s", 1) == 0 || strncmp(unit_start, "sec", 3) == 0 ||
			   strncmp(unit_start, "secs", 4) == 0 || strncmp(unit_start, "second", 6) == 0 ||
			   strncmp(unit_start, "seconds", 7) == 0) {
			multiplier = 1;
		} else if (strncmp(unit_start, "m", 1) == 0 || strncmp(unit_start, "min", 3) == 0 ||
			   strncmp(unit_start, "mins", 4) == 0 || strncmp(unit_start, "minute", 6) == 0 ||
			   strncmp(unit_start, "minutes", 7) == 0) {
			multiplier = 60;
		} else if (strncmp(unit_start, "h", 1) == 0 || strncmp(unit_start, "hr", 2) == 0 ||
			   strncmp(unit_start, "hrs", 3) == 0 || strncmp(unit_start, "hour", 4) == 0 ||
			   strncmp(unit_start, "hours", 5) == 0) {
			multiplier = 3600;
		} else if (strncmp(unit_start, "d", 1) == 0 || strncmp(unit_start, "day", 3) == 0 ||
			   strncmp(unit_start, "days", 4) == 0) {
			multiplier = 86400;
		} else if (strncmp(unit_start, "w", 1) == 0 || strncmp(unit_start, "week", 4) == 0 ||
			   strncmp(unit_start, "weeks", 5) == 0) {
			multiplier = 604800;
		} else {
			return (time_t)-1;
		}

		time_t now = mxrec_now();
		if (now == (time_t)-1)
			return (time_t)-1;

		if (value > 0 && (unsigned long)value > (unsigned long)(now / multiplier))
			return 0;

		return now - (time_t)(value * multiplier);
	}
}
