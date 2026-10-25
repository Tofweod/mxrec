#include "utils/time.h"
#include <stdio.h>

static int check_durations(void)
{
	const struct {
		const char *input;
		time_t expected;
	} cases[] = {
		{"0", 0},
		{"604800", 604800},
		{"10s", 10},
		{"5min", 300},
		{"2h", 7200},
		{"1hours", 3600},
		{"1day", 86400},
		{"1week", 604800},
		{"3weeks", 1814400},
		{" 30  sec ", 30},
		{"365d", 31536000},
		{NULL, -1},
		{"", -1},
		{" ", -1},
		{"bad", -1},
		{"-1week", -1},
		{"1.5h", -1},
		{"1month", -1},
		{"1weeksjunk", -1},
		{"1sec extra", -1},
		{"2021-01-01", -1},
		{"2021-01-01 12:30:00", -1},
		{"999999999999999999999999999999s", -1},
		{"9223372036854775807weeks", -1},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		time_t actual = string2timestamp(cases[i].input);
		if (actual != cases[i].expected) {
			fprintf(stderr, "duration %s: expected %lld, got %lld\n",
				cases[i].input ? cases[i].input : "(null)", (long long)cases[i].expected,
				(long long)actual);
			return 1;
		}
	}

	puts("Duration parsing checks passed");
	return 0;
}

int main(void) { return check_durations(); }
