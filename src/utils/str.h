#ifndef TOF_MXREC_UTIL_STR_H
#define TOF_MXREC_UTIL_STR_H

#include "assert.h"
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

char *parseKVFormat(const char *fmt, ...);

char *parseFormat(const char *fmt, ...);

#endif // !TOF_MXREC_UTIL_STR_H
