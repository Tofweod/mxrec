#include "time.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
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
	static const struct {
		const char *name;
		uintmax_t seconds;
	} units[] = {
		{"s", 1},      {"sec", 1},	 {"secs", 1},	    {"second", 1},   {"seconds", 1}, {"m", 60},
		{"min", 60},   {"mins", 60},	 {"minute", 60},    {"minutes", 60}, {"h", 3600},    {"hr", 3600},
		{"hrs", 3600}, {"hour", 3600},	 {"hours", 3600},   {"d", 86400},    {"day", 86400}, {"days", 86400},
		{"w", 604800}, {"week", 604800}, {"weeks", 604800},
	};
	char *end;
	uintmax_t value, multiplier = 1;
	size_t unit_len;
	time_t seconds;

	if (!str)
		return (time_t)-1;
	while (isspace((unsigned char)*str))
		str++;
	if (!isdigit((unsigned char)*str))
		return (time_t)-1;

	errno = 0;
	value = strtoumax(str, &end, 10);
	if (errno == ERANGE)
		return (time_t)-1;
	while (isspace((unsigned char)*end))
		end++;
	const char *unit = end;
	while (*end && !isspace((unsigned char)*end))
		end++;
	unit_len = end - unit;
	while (isspace((unsigned char)*end))
		end++;
	if (*end)
		return (time_t)-1;

	if (unit_len) {
		multiplier = 0;
		for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); i++) {
			if (strlen(units[i].name) == unit_len && memcmp(unit, units[i].name, unit_len) == 0) {
				multiplier = units[i].seconds;
				break;
			}
		}
		if (!multiplier)
			return (time_t)-1;
	}
	if (value > (uintmax_t)INTMAX_MAX / multiplier)
		return (time_t)-1;
	value *= multiplier;
	seconds = (time_t)value;
	if (seconds < 0 || (uintmax_t)seconds != value)
		return (time_t)-1;
	return seconds;
}
