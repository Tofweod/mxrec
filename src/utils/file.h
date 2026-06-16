#ifndef TOF_MXREC_UTIL_FILE_H
#define TOF_MXREC_UTIL_FILE_H

/* Read entire file into a heap-allocated string (NUL-terminated).
 * Trailing newline / carriage return is stripped.
 * Returns NULL on failure (file not found, read error, empty file).
 * Caller must xfree() the returned pointer. */
#include <stdio.h>
char* slurp(const char *path);

/* Output a C string as a JSON string value to fp,
 * including surrounding double quotes. Characters ", \, \n, \r, \t
 * and control chars (0x00-0x1F) are properly escaped. */
void fprintf_json_str(FILE *fp, const char* s);

#endif
