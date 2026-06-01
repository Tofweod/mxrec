#include "utils/string.h"
#include "bb.h"
#include "comm.h"
#include "xmalloc.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#define DEFAULT_FORMAT_CAP 8

#define ISKVEND(kv) ((kv)->key == NULL)

__BB_INIT(static, StrBuffer, _sb, char);
__BB_FREE(static, StrBuffer, _sb);
__BB_CLEAR(static, StrBuffer, _sb);
__BB_REVERSE(static, StrBuffer, _sb, char);
__BB_APPEND(static, StrBuffer, _sb, char);

void sb_init(StrBuffer *sb, size_t cap)
{
	_sb_init(sb, cap);
	sb->buf[0] = '\0';
}

void sb_free(StrBuffer *sb) { _sb_free(sb); }

void sb_clear(StrBuffer *sb) { _sb_clear(sb); }

void sb_append_str(StrBuffer *sb, const char *s)
{
	size_t n = strlen(s);
	_sb_append(sb, s, n + 1);
	// null terminal
	sb->buf[--(sb->len)] = '\0';
}

void sb_append_char(StrBuffer *sb, int ch)
{
	_sb_append(sb, &ch, 1);
	sb->buf[sb->len] = '\0';
}

static const char *kvgetkey(kv_t *kvs, const char *key)
{
	kv_t *cur = kvs;
	while (!ISKVEND(cur)) {
		if (strcmp(cur->key, key) == 0) {
			return cur->val;
		}
		++cur;
	}
	return NULL;
}

static int __parseKVFormat(const char *fmt, kv_t *kvs, StrBuffer *sb)
{
	const char *p = fmt;
	char key[MAX_KEY_SIZE + 1];
	while (*p) {
		if (*p == '{') {
			const char *start = ++p;

			while (*p && *p != '}')
				++p;

			size_t len = p - start;

			strncpy(key, start, len);
			key[len] = '\0';

			const char *val = kvgetkey(kvs, key);
			if (val)
				sb_append_str(sb, val);
			if (*p == '}')
				++p;
		} else {
			sb_append_char(sb, *p);
			p++;
		}
	}
	return 0;
}

char *parseKVFormat(const char *fmt, ...)
{
	va_list args;
	kv_t *pairs;
	size_t n = 0, cap = DEFAULT_FORMAT_CAP;
	StrBuffer sb;
	char *ret;

	va_start(args, fmt);

	pairs = xmalloc(sizeof(kv_t) * cap);

	while (1) {
		kv_t kv = va_arg(args, kv_t);

		if (ISKVEND(&kv))
			break;

		if (n >= cap) {
			cap *= 2;
			pairs = xrealloc(pairs, sizeof(kv_t) * cap);
		}

		pairs[n++] = kv;
	}

	va_end(args);

	pairs[n] = KV_END;

	sb_init(&sb, strlen(fmt));

	if (__parseKVFormat(fmt, pairs, &sb) < 0)
		mxrec_cleanup(cleanup, ret, 0);

	ret = sb.buf;
cleanup:
	xfree(pairs);

	return ret;
}

char *parseFormat(const char *fmt, ...)
{
	char *result;
	va_list ap;
	va_start(ap, fmt);
	result = parsevFormat(fmt, ap);
	va_end(ap);
	return result;
}

char *parsevFormat(const char *fmt, va_list ap)
{
	char *s;
	xvasprintf(&s, fmt, ap);
	return s;
}

// **COPY FROM REDIS SOURCE CODE**

/* Return the number of digits of 'v' when converted to string in radix 10.
 * See ll2string() for more information. */
uint32_t digits10(uint64_t v)
{
	if (v < 10)
		return 1;
	if (v < 100)
		return 2;
	if (v < 1000)
		return 3;
	if (v < 1000000000000UL) {
		if (v < 100000000UL) {
			if (v < 1000000) {
				if (v < 10000)
					return 4;
				return 5 + (v >= 100000);
			}
			return 7 + (v >= 10000000UL);
		}
		if (v < 10000000000UL) {
			return 9 + (v >= 1000000000UL);
		}
		return 11 + (v >= 100000000000UL);
	}
	return 12 + digits10(v / 1000000000000UL);
}

/* Like digits10() but for signed values. */
uint32_t sdigits10(int64_t v)
{
	if (v < 0) {
		/* Abs value of LLONG_MIN requires special handling. */
		uint64_t uv = (v != LLONG_MIN) ? (uint64_t)-v : ((uint64_t)LLONG_MAX) + 1;
		return digits10(uv) + 1; /* +1 for the minus. */
	} else {
		return digits10(v);
	}
}

/* Convert a long long into a string. Returns the number of
 * characters needed to represent the number.
 * If the buffer is not big enough to store the string, 0 is returned. */
int ll2string(char *dst, size_t dstlen, long long svalue)
{
	unsigned long long value;
	int negative = 0;

	/* The ull2string function with 64bit unsigned integers for simplicity, so
	 * we convert the number here and remember if it is negative. */
	if (svalue < 0) {
		if (svalue != LLONG_MIN) {
			value = -svalue;
		} else {
			value = ((unsigned long long)LLONG_MAX) + 1;
		}
		if (dstlen < 2)
			goto err;
		negative = 1;
		dst[0] = '-';
		dst++;
		dstlen--;
	} else {
		value = svalue;
	}

	/* Converts the unsigned long long value to string*/
	int length = ull2string(dst, dstlen, value);
	if (length == 0)
		return 0;
	return length + negative;

err:
	/* force add Null termination */
	if (dstlen > 0)
		dst[0] = '\0';
	return 0;
}

/* Convert a unsigned long long into a string. Returns the number of
 * characters needed to represent the number.
 * If the buffer is not big enough to store the string, 0 is returned.
 *
 * Based on the following article (that apparently does not provide a
 * novel approach but only publicizes an already used technique):
 *
 * https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920 */
int ull2string(char *dst, size_t dstlen, unsigned long long value)
{
	static const char digits[201] = "0001020304050607080910111213141516171819"
					"2021222324252627282930313233343536373839"
					"4041424344454647484950515253545556575859"
					"6061626364656667686970717273747576777879"
					"8081828384858687888990919293949596979899";

	/* Check length. */
	uint32_t length = digits10(value);
	if (length >= dstlen)
		goto err;
	;

	/* Null term. */
	uint32_t next = length - 1;
	dst[next + 1] = '\0';
	while (value >= 100) {
		int const i = (value % 100) * 2;
		value /= 100;
		dst[next] = digits[i + 1];
		dst[next - 1] = digits[i];
		next -= 2;
	}

	/* Handle last 1-2 digits. */
	if (value < 10) {
		dst[next] = '0' + (uint32_t)value;
	} else {
		int i = (uint32_t)value * 2;
		dst[next] = digits[i + 1];
		dst[next - 1] = digits[i];
	}
	return length;
err:
	/* force add Null termination */
	if (dstlen > 0)
		dst[0] = '\0';
	return 0;
}

/* Convert a string into a long long. Returns 1 if the string could be parsed
 * into a (non-overflowing) long long, 0 otherwise. The value will be set to
 * the parsed value when appropriate.
 *
 * Note that this function demands that the string strictly represents
 * a long long: no spaces or other characters before or after the string
 * representing the number are accepted, nor zeroes at the start if not
 * for the string "0" representing the zero number.
 *
 * Because of its strictness, it is safe to use this function to check if
 * you can convert a string into a long long, and obtain back the string
 * from the number without any loss in the string representation. */
int string2ll(const char *s, size_t slen, long long *value)
{
	const char *p = s;
	size_t plen = 0;
	int negative = 0;
	unsigned long long v;

	/* A string of zero length or excessive length is not a valid number. */
	if (plen == slen || slen >= LONG_STR_SIZE)
		return 0;

	/* Special case: first and only digit is 0. */
	if (slen == 1 && p[0] == '0') {
		if (value != NULL)
			*value = 0;
		return 1;
	}

	/* Handle negative numbers: just set a flag and continue like if it
	 * was a positive number. Later convert into negative. */
	if (p[0] == '-') {
		negative = 1;
		p++;
		plen++;

		/* Abort on only a negative sign. */
		if (plen == slen)
			return 0;
	}

	/* First digit should be 1-9, otherwise the string should just be 0. */
	if (p[0] >= '1' && p[0] <= '9') {
		v = p[0] - '0';
		p++;
		plen++;
	} else {
		return 0;
	}

	/* Parse all the other digits, checking for overflow at every step. */
	while (plen < slen && p[0] >= '0' && p[0] <= '9') {
		if (v > (ULLONG_MAX / 10)) /* Overflow. */
			return 0;
		v *= 10;

		if (v > (ULLONG_MAX - (p[0] - '0'))) /* Overflow. */
			return 0;
		v += p[0] - '0';

		p++;
		plen++;
	}

	/* Return if not all bytes were used. */
	if (plen < slen)
		return 0;

	/* Convert to negative if needed, and do the final overflow check when
	 * converting from unsigned long long to long long. */
	if (negative) {
		if (v > ((unsigned long long)(-(LLONG_MIN + 1)) + 1)) /* Overflow. */
			return 0;
		if (value != NULL)
			*value = -v;
	} else {
		if (v > LLONG_MAX) /* Overflow. */
			return 0;
		if (value != NULL)
			*value = v;
	}
	return 1;
}

/* Helper function to convert a string to an unsigned long long value.
 * The function attempts to use the faster string2ll() function inside
 * Redis: if it fails, strtoull() is used instead. The function returns
 * 1 if the conversion happened successfully or 0 if the number is
 * invalid or out of range. */
int string2ull(const char *s, unsigned long long *value)
{
	long long ll;
	if (string2ll(s, strlen(s), &ll)) {
		if (ll < 0)
			return 0; /* Negative values are out of range. */
		*value = ll;
		return 1;
	}
	errno = 0;
	char *endptr = NULL;
	*value = strtoull(s, &endptr, 10);
	if (errno == EINVAL || errno == ERANGE || !(*s != '\0' && *endptr == '\0'))
		return 0; /* strtoull() failed. */
	return 1;	  /* Conversion done! */
}

/* Convert a string into a long. Returns 1 if the string could be parsed into a
 * (non-overflowing) long, 0 otherwise. The value will be set to the parsed
 * value when appropriate. */
int string2l(const char *s, size_t slen, long *lval)
{
	long long llval;

	if (!string2ll(s, slen, &llval))
		return 0;

	if (llval < LONG_MIN || llval > LONG_MAX)
		return 0;

	*lval = (long)llval;
	return 1;
}

/* return 1 if c>= start && c <= end, 0 otherwise*/
static int safe_is_c_in_range(char c, char start, char end)
{
	if (c >= start && c <= end)
		return 1;
	return 0;
}

static int base_16_char_type(char c)
{
	if (safe_is_c_in_range(c, '0', '9'))
		return 0;
	if (safe_is_c_in_range(c, 'a', 'f'))
		return 1;
	if (safe_is_c_in_range(c, 'A', 'F'))
		return 2;
	return -1;
}

/** This is an async-signal safe version of string2l to convert unsigned long to string.
 * The function translates @param src until it reaches a value that is not 0-9, a-f or A-F, or @param we read slen
 * characters. On successes writes the result to @param result_output and returns 1. if the string represents an
 * overflow value, return -1. */
int string2ul_base16_async_signal_safe(const char *src, size_t slen, unsigned long *result_output)
{
	static char ascii_to_dec[] = {'0', 'a' - 10, 'A' - 10};

	int char_type = 0;
	size_t curr_char_idx = 0;
	unsigned long result = 0;
	int base = 16;
	while ((-1 != (char_type = base_16_char_type(src[curr_char_idx]))) && curr_char_idx < slen) {
		unsigned long curr_val = src[curr_char_idx] - ascii_to_dec[char_type];
		if ((result > ULONG_MAX / base) || (result > (ULONG_MAX - curr_val) / base)) /* Overflow. */
			return -1;
		result = result * base + curr_val;
		++curr_char_idx;
	}

	*result_output = result;
	return 1;
}

/* Convert a string into a double. Returns 1 if the string could be parsed
 * into a (non-overflowing) double, 0 otherwise. The value will be set to
 * the parsed value when appropriate.
 *
 * Note that this function demands that the string strictly represents
 * a double: no spaces or other characters before or after the string
 * representing the number are accepted. */
int string2ld(const char *s, size_t slen, long double *dp)
{
	char buf[MAX_LONG_DOUBLE_CHARS];
	long double value;
	char *eptr;

	if (slen == 0 || slen >= sizeof(buf))
		return 0;
	memcpy(buf, s, slen);
	buf[slen] = '\0';

	errno = 0;
	value = strtold(buf, &eptr);
	if (isspace(buf[0]) || eptr[0] != '\0' || (size_t)(eptr - buf) != slen ||
	    (errno == ERANGE && (value == HUGE_VAL || value == -HUGE_VAL || fpclassify(value) == FP_ZERO)) ||
	    errno == EINVAL || isnan(value))
		return 0;

	if (dp)
		*dp = value;
	return 1;
}

/* Convert a string into a double. Returns 1 if the string could be parsed
 * into a (non-overflowing) double, 0 otherwise. The value will be set to
 * the parsed value when appropriate.
 *
 * Note that this function demands that the string strictly represents
 * a double: no spaces or other characters before or after the string
 * representing the number are accepted. */
int string2d(const char *s, size_t slen, double *dp)
{
	errno = 0;
	/* Fast path to reject empty strings, or strings starting by space explicitly */
	if (unlikely(slen == 0 || isspace(((const char *)s)[0])))
		return 0;
	char *fallback_eptr;
	*dp = strtod(s, &fallback_eptr);
	if (*fallback_eptr != '\0')
		return 0;
	if (unlikely(errno == EINVAL ||
		     (errno == ERANGE && (*dp == HUGE_VAL || *dp == -HUGE_VAL || fpclassify(*dp) == FP_ZERO)) ||
		     isnan(*dp)))
		return 0;
	return 1;
}

/* Returns 1 if the double value can safely be represented in long long without
 * precision loss, in which case the corresponding long long is stored in the out variable. */
int double2ll(double d, long long *out)
{
#if (DBL_MANT_DIG >= 52) && (DBL_MANT_DIG <= 63) && (LLONG_MAX == 0x7fffffffffffffffLL)
	/* Check if the float is in a safe range to be casted into a
	 * long long. We are assuming that long long is 64 bit here.
	 * Also we are assuming that there are no implementations around where
	 * double has precision < 52 bit.
	 *
	 * Under this assumptions we test if a double is inside a range
	 * where casting to long long is safe. Then using two castings we
	 * make sure the decimal part is zero. If all this is true we can use
	 * integer without precision loss.
	 *
	 * Note that numbers above 2^52 and below 2^63 use all the fraction bits as real part,
	 * and the exponent bits are positive, which means the "decimal" part must be 0.
	 * i.e. all double values in that range are representable as a long without precision loss,
	 * but not all long values in that range can be represented as a double.
	 * we only care about the first part here. */
	if (d < (double)(-LLONG_MAX / 2) || d > (double)(LLONG_MAX / 2))
		return 0;
	long long ll = d;
	if (ll == d) {
		*out = ll;
		return 1;
	}
#endif
	return 0;
}

/* Convert a double into a string with 'fractional_digits' digits after the dot precision.
 * This is an optimized version of snprintf "%.<fractional_digits>f".
 * We convert the double to long and multiply it  by 10 ^ <fractional_digits> to shift
 * the decimal places.
 * Note that multiply it of input value by 10 ^ <fractional_digits> can overflow but on the scenario
 * that we currently use within redis this that is not possible.
 * After we get the long representation we use the logic from ull2string function on this file
 * which is based on the following article:
 * https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920
 *
 * Input values:
 * char: the buffer to store the string representation
 * dstlen: the buffer length
 * dvalue: the input double
 * fractional_digits: the number of fractional digits after the dot precision. between 1 and 17
 *
 * Return values:
 * Returns the number of characters needed to represent the number.
 * If the buffer is not big enough to store the string, 0 is returned.
 */
int fixedpoint_d2string(char *dst, size_t dstlen, double dvalue, int fractional_digits)
{
	if (fractional_digits < 1 || fractional_digits > 17)
		goto err;
	/* min size of 2 ( due to 0. ) + n fractional_digitits + \0 */
	if ((int)dstlen < (fractional_digits + 3))
		goto err;
	if (dvalue == 0) {
		dst[0] = '0';
		dst[1] = '.';
		memset(dst + 2, '0', fractional_digits);
		dst[fractional_digits + 2] = '\0';
		return fractional_digits + 2;
	}
	/* scale and round */
	static double powers_of_ten[] = {1.0,
					 10.0,
					 100.0,
					 1000.0,
					 10000.0,
					 100000.0,
					 1000000.0,
					 10000000.0,
					 100000000.0,
					 1000000000.0,
					 10000000000.0,
					 100000000000.0,
					 1000000000000.0,
					 10000000000000.0,
					 100000000000000.0,
					 1000000000000000.0,
					 10000000000000000.0,
					 100000000000000000.0};
	long long svalue = llrint(dvalue * powers_of_ten[fractional_digits]);
	unsigned long long value;
	/* write sign */
	int negative = 0;
	if (svalue < 0) {
		if (svalue != LLONG_MIN) {
			value = -svalue;
		} else {
			value = ((unsigned long long)LLONG_MAX) + 1;
		}
		if (dstlen < 2)
			goto err;
		negative = 1;
		dst[0] = '-';
		dst++;
		dstlen--;
	} else {
		value = svalue;
	}

	static const char digitsd[201] = "0001020304050607080910111213141516171819"
					 "2021222324252627282930313233343536373839"
					 "4041424344454647484950515253545556575859"
					 "6061626364656667686970717273747576777879"
					 "8081828384858687888990919293949596979899";

	/* Check length. */
	uint32_t ndigits = digits10(value);
	if (ndigits >= dstlen)
		goto err;
	int integer_digits = ndigits - fractional_digits;
	/* Fractional only check to avoid representing 0.7750 as .7750.
	 * This means we need to increment the length and store 0 as the first character.
	 */
	if (integer_digits < 1) {
		dst[0] = '0';
		integer_digits = 1;
	}
	dst[integer_digits] = '.';
	int size = integer_digits + 1 + fractional_digits;
	/* fill with 0 from fractional digits until size */
	memset(dst + integer_digits + 1, '0', fractional_digits);
	int next = size - 1;
	while (value >= 100) {
		int const i = (value % 100) * 2;
		value /= 100;
		dst[next] = digitsd[i + 1];
		dst[next - 1] = digitsd[i];
		next -= 2;
		/* dot position */
		if (next == integer_digits) {
			next--;
		}
	}

	/* Handle last 1-2 digits. */
	if (value < 10) {
		dst[next] = '0' + (uint32_t)value;
	} else {
		int i = (uint32_t)value * 2;
		dst[next] = digitsd[i + 1];
		dst[next - 1] = digitsd[i];
	}
	/* Null term. */
	dst[size] = '\0';
	return size + negative;
err:
	/* force add Null termination */
	if (dstlen > 0)
		dst[0] = '\0';
	return 0;
}

/* Trims off trailing zeros from a string representing a double. */
int trimDoubleString(char *buf, size_t len)
{
	if (strchr(buf, '.') != NULL) {
		char *p = buf + len - 1;
		while (*p == '0') {
			p--;
			len--;
		}
		if (*p == '.')
			len--;
	}
	buf[len] = '\0';
	return len;
}

/* Create a string object from a long double.
 * If mode is humanfriendly it does not use exponential format and trims trailing
 * zeroes at the end (may result in loss of precision).
 * If mode is default exp format is used and the output of snprintf()
 * is not modified (may result in loss of precision).
 * If mode is hex hexadecimal format is used (no loss of precision)
 *
 * The function returns the length of the string or zero if there was not
 * enough buffer room to store it. */
int ld2string(char *buf, size_t len, long double value, ld2string_mode mode)
{
	size_t l = 0;

	if (isinf(value)) {
		/* Libc in odd systems (Hi Solaris!) will format infinite in a
		 * different way, so better to handle it in an explicit way. */
		if (len < 5)
			goto err; /* No room. 5 is "-inf\0" */
		if (value > 0) {
			memcpy(buf, "inf", 3);
			l = 3;
		} else {
			memcpy(buf, "-inf", 4);
			l = 4;
		}
	} else if (isnan(value)) {
		/* Libc in some systems will format nan in a different way,
		 * like nan, -nan, NAN, nan(char-sequence).
		 * So we normalize it and create a single nan form in an explicit way. */
		if (len < 4)
			goto err; /* No room. 4 is "nan\0" */
		memcpy(buf, "nan", 3);
		l = 3;
	} else {
		switch (mode) {
		case LD_STR_AUTO:
			l = snprintf(buf, len, "%.17Lg", value);
			if (l + 1 > len)
				goto err;
			; /* No room. */
			break;
		case LD_STR_HEX:
			l = snprintf(buf, len, "%La", value);
			if (l + 1 > len)
				goto err; /* No room. */
			break;
		case LD_STR_HUMAN:
			/* We use 17 digits precision since with 128 bit floats that precision
			 * after rounding is able to represent most small decimal numbers in a
			 * way that is "non surprising" for the user (that is, most small
			 * decimal numbers will be represented in a way that when converted
			 * back into a string are exactly the same as what the user typed.) */
			l = snprintf(buf, len, "%.17Lf", value);
			if (l + 1 > len)
				goto err; /* No room. */
			/* Now remove trailing zeroes after the '.' */
			if (strchr(buf, '.') != NULL) {
				char *p = buf + l - 1;
				while (*p == '0') {
					p--;
					l--;
				}
				if (*p == '.')
					l--;
			}
			if (l == 2 && buf[0] == '-' && buf[1] == '0') {
				buf[0] = '0';
				l = 1;
			}
			break;
		default:
			goto err; /* Invalid mode. */
		}
	}
	buf[l] = '\0';
	return l;
err:
	/* force add Null termination */
	if (len > 0)
		buf[0] = '\0';
	return 0;
}
