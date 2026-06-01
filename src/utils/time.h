#ifndef TOF_MXREC_UTIL_TIME_H
#define TOF_MXREC_UTIL_TIME_H

#include <time.h>

time_t mxrec_now(void);

/**
 * parse_since_time - Parse a time specification string into a Unix timestamp
 *                    (seconds). The function first tries to interpret the
 *                    string as an absolute time, then falls back to a relative
 *                    time in the past.
 *
 * @param str  Input string, may be an absolute time like "2021-01-01 12:30:00"
 *             or a relative time like "2h", "30min", "1day" (always in the
 *             past). Leading/trailing whitespace is allowed.
 * @return     time_t timestamp on success, or (time_t)-1 on failure.
 *
 * Absolute time formats (local time):
 *   "2021-01-01"
 *   "2021-01-01 12:30:00"
 *   "2021-01-01T12:30:00"
 *   "2021-01-01T12:30:00.123456"  (fractional seconds truncated)
 *
 * Relative time formats (always in the past):
 *   "<number>[ <unit> ]"
 *   If the unit is omitted, seconds are assumed ("0" → 0 seconds ago).
 *   Supported units: s, sec, secs, second, seconds,
 *                    m, min, mins, minute, minutes,
 *                    h, hr, hrs, hour, hours,
 *                    d, day, days,
 *                    w, week, weeks
 */
time_t string2timestamp(const char *str);

#endif // !TOF_MXREC_UTIL_TIME_H
