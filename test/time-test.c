#define _POSIX_C_SOURCE 200809L
#include "utils/time.h"
#include <stdio.h>
#include <stdlib.h>

static int check(const char *input, time_t expected)
{
	time_t actual = string2timestamp(input);
	if (actual == expected)
		return 0;
	fprintf(stderr, "time %s: expected %lld, got %lld\n", input ? input : "(null)", (long long)expected,
		(long long)actual);
	return 1;
}

static int check_relative(void)
{
	const struct {
		const char *input;
		time_t seconds;
	} cases[] = {
		{"0", 0},	    {"604800", 604800}, {"10s", 10},	   {"5min", 300},	{"2h", 7200},
		{"1hours", 3600},   {"1day", 86400},	{"1week", 604800}, {"3weeks", 1814400}, {" 30  sec ", 30},
		{"365d", 31536000},
	};
	time_t now = mxrec_now();
	if (now == (time_t)-1)
		return 1;
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
		if (check(cases[i].input, now - cases[i].seconds))
			return 1;
	}
	/* Large, valid lookbacks keep the original epoch-zero lower bound. */
	char beyond_epoch[64];
	snprintf(beyond_epoch, sizeof(beyond_epoch), "%llds", (long long)now + 1);
	return check(beyond_epoch, 0);
}

static int check_absolute(void)
{
	const struct {
		const char *input;
		time_t timestamp;
	} cases[] = {
		{"1970-01-01", 0},
		{"1960-01-01", -315619200},
		{"2021-01-01", 1609459200},
		{"2021-01-01 12:30:00", 1609504200},
		{"2021-01-01T12:30:00", 1609504200},
		{"2021-01-01T12:30:00.123456", 1609504200},
		{"2021-01-01 12:30:00.9", 1609504200},
		{" \t2021-01-01 12:30:00 \n", 1609504200},
		{"2024-02-29", 1709164800},
		{"2040-01-01", 2208988800LL},
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
		if (check(cases[i].input, cases[i].timestamp))
			return 1;
	}
	/* Absolute inputs follow local time; relative lookbacks do not use TZ. */
	if (setenv("TZ", "UTC-8", 1))
		return 1;
	tzset();
	return check("2021-01-01 12:30:00", 1609475400) || check_relative();
}

static int check_invalid(void)
{
	const char *cases[] = {
		NULL,
		"",
		" ",
		"bad",
		"-1week",
		"+1week",
		"1.5h",
		"1month",
		"1weeksjunk",
		"1sec extra",
		"999999999999999999999999999999s",
		"9223372036854775807weeks",
		"18446744073709551615s",
		"2021-01-01junk",
		"2021-01-01.123",
		"2021-01-01T12:30:00.",
		"2021-01-01T12:30:00.abc",
		"2021-01-01T12:30:00.123junk",
		"2021-13-01",
		"2021-01-32",
		"2021-02-29",
		"2024-02-30",
		"2021-01-01 24:00:00",
		"2021-01-01 12:60:00",
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
		if (check(cases[i], (time_t)-1))
			return 1;
	}
	return 0;
}

int main(void)
{
	if (setenv("TZ", "UTC0", 1))
		return 1;
	tzset();
	if (check_relative() || check_absolute() || check_invalid())
		return 1;
	puts("Timestamp parsing checks passed");
	return 0;
}
