#ifndef TOF_MXREC_UTIL_STR_H
#define TOF_MXREC_UTIL_STR_H

#include "assert.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
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

// stringbuilder
typedef struct StrBuffer {
	size_t len;
	size_t cap;
	char *buf;
} StrBuffer;

void sb_init(StrBuffer *sb, size_t cap);
void sb_free(StrBuffer *sb);
void sb_clear(StrBuffer *sb);
void sb_append_str(StrBuffer *sb, const char *s);
void sb_append_char(StrBuffer *sb, int ch);

// transfrom
// **COPY FROM REDIS SOURCE CODE**

/* The maximum number of characters needed to represent a long double
 * as a string (long double has a huge range of some 4952 chars, see LDBL_MAX).
 * This should be the size of the buffer given to ld2string */
#define MAX_LONG_DOUBLE_CHARS 5*1024

/* The maximum number of characters needed to represent a double
 * as a string (double has a huge range of some 328 chars, see DBL_MAX).
 * This should be the size of the buffer for sprintf with %f */
#define MAX_DOUBLE_CHARS 400

/* The maximum number of characters needed to for d2string/fpconv_dtoa call.
 * Since it uses %g and not %f, some 40 chars should be enough. */
#define MAX_D2STRING_CHARS 128

/* Bytes needed for long -> str + '\0' */
#define LONG_STR_SIZE 21

/* long double to string conversion options */
typedef enum {
	LD_STR_AUTO,  /* %.17Lg */
	LD_STR_HUMAN, /* %.17Lf + Trimming of trailing zeros */
	LD_STR_HEX    /* %La */
} ld2string_mode;
uint32_t digits10(uint64_t v);
uint32_t sdigits10(int64_t v);
int ll2string(char *s, size_t len, long long value);
int ull2string(char *s, size_t len, unsigned long long value);
int string2ll(const char *s, size_t slen, long long *value);
int string2ull(const char *s, unsigned long long *value);
int string2l(const char *s, size_t slen, long *value);
int string2ul_base16_async_signal_safe(const char *src, size_t slen, unsigned long *result_output);
int string2ld(const char *s, size_t slen, long double *dp);
// TODO
int string2d(const char *s, size_t slen, double *dp);
int trimDoubleString(char *buf, size_t len);
// TODO
int d2string(char *buf, size_t len, double value);
int fixedpoint_d2string(char *dst, size_t dstlen, double dvalue, int fractional_digits);
int ld2string(char *buf, size_t len, long double value, ld2string_mode mode);
int double2ll(double d, long long *out);

#endif // !TOF_MXREC_UTIL_STR_H
