#ifndef TOF_MXREC_UTIL_STR_H
#define TOF_MXREC_UTIL_STR_H

#include "assert.h"
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#define MAX_KEY_SIZE 50

typedef struct {
	const char *key;
	const char *val;
} kv_t;

#define MAKE_KV(k, v)                       \
	(assert(strlen(k) <= MAX_KEY_SIZE), \
	 (kv_t){.key = (k), .val = (v)})

#define KV_END (kv_t){0}

// return pointer need to free manually
char *parseKVFormat(const char *fmt, ...);

// return pointer need to free manually
char *parsevFormat(const char *fmt, va_list ap);

// return pointer need to free manually
char *parseFormat(const char *fmt, ...);

#endif // !TOF_MXREC_UTIL_STR_H
