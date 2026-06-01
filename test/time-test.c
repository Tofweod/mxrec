#include "utils/time.h"
#include <stdio.h>

int main()
{
	const char *tests[] = {"0",
			       "10s",
			       "5min",
			       "2h",
			       "1day",
			       "3weeks",
			       " 30  sec",
			       "1hours",
			       "bad",
			       "365d",
			       "2021-01-01",
			       "2021-01-01 12:30:00",
			       "2021-01-01T12:30:00",
			       "2021-01-01T12:30:00.123456",
			       NULL};

	for (int i = 0; tests[i]; i++) {
		time_t t = string2timestamp(tests[i]);
		if (t == (time_t)-1) {
			printf("%-15s → parse failed\n", tests[i]);
		} else {
			struct tm *lt = localtime(&t);
			char buf[64];
			strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
			printf("%-15s → %lld (%s)\n", tests[i], (long long)t, buf);
		}
	}
	return 0;
}
