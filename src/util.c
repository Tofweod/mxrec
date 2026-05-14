#include "util.h"
#include <stdarg.h>

static char *parsevFormat(const char *fmt, va_list ap)
{
}

static char *parseKV(const char *src, const char *key, const char *val)
{
}

static char *parseVKVFormat(const char *src, va_list ap)
{
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

char *parseKVFormat(const char *src, ...)
{
	char *result;
	va_list ap;
	va_start(ap, src);
	result = parseVKVFormat(src, ap);
	va_end(ap);
	return result;
}
