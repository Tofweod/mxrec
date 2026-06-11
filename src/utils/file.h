#ifndef TOF_MXREC_UTIL_FILE_H
#define TOF_MXREC_UTIL_FILE_H

/* Read entire file into a heap-allocated string (NUL-terminated).
 * Trailing newline / carriage return is stripped.
 * Returns NULL on failure (file not found, read error, empty file).
 * Caller must xfree() the returned pointer. */
char* slurp(const char *path);

#endif
