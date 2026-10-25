#ifndef TOF_MXREC_UTIL_TIME_H
#define TOF_MXREC_UTIL_TIME_H

#include <time.h>

time_t mxrec_now(void);

/**
 * Parse a nonnegative duration into seconds, e.g. "1week" -> 604800.
 * Despite the historical name, the result is a duration, not a Unix timestamp.
 * Leading/trailing whitespace is allowed; bare integers mean seconds.
 * Returns (time_t)-1 for invalid input or overflow. Absolute dates are invalid.
 *
 * Format: "<number>[ <unit> ]"
 *   Supported units: s, sec, secs, second, seconds,
 *                    m, min, mins, minute, minutes,
 *                    h, hr, hrs, hour, hours,
 *                    d, day, days,
 *                    w, week, weeks
 */
time_t string2timestamp(const char *str);

#endif // !TOF_MXREC_UTIL_TIME_H
