#ifndef TOF_MXREC_UTIL_TIME_H
#define TOF_MXREC_UTIL_TIME_H

#include <time.h>

time_t mxrec_now(void);

/**
 * Parse an absolute local date/time or a relative lookback into a Unix timestamp.
 * For example, "1week" returns mxrec_now() - 604800, and "0" returns mxrec_now().
 * Leading/trailing whitespace is allowed; bare integers mean seconds ago.
 * Returns (time_t)-1 for invalid input or overflow. Relative results are clamped
 * at Unix epoch zero when the lookback exceeds the current timestamp.
 *
 * Absolute formats (local time): YYYY-MM-DD, YYYY-MM-DD HH:MM:SS,
 * YYYY-MM-DDTHH:MM:SS. Date-times may have a fractional second suffix; it is
 * truncated. Invalid calendar dates and trailing text are rejected.
 * Relative format: "<nonnegative number>[ <unit> ]"
 *   Supported units: s, sec, secs, second, seconds,
 *                    m, min, mins, minute, minutes,
 *                    h, hr, hrs, hour, hours,
 *                    d, day, days,
 *                    w, week, weeks
 */
time_t string2timestamp(const char *str);

#endif // !TOF_MXREC_UTIL_TIME_H
