#include "assert.h"
#include "comm.h"
#include <stdarg.h>
#include <stdio.h>

#define MAX_LOG_SIZE 1024

static char log_buf[MAX_LOG_SIZE];

static inline void _printflevel(const char *level, const char *msg)
{
	printf("[%s]:%s\n", level, msg);
}

noinline void _Assert(const char *estr, const char *file, int line)
{
	snprintf(log_buf, sizeof(log_buf), "%s failed in %s:%d", estr, file, line);
	_printflevel("Assert", log_buf);
}

noinline void _Panic(const char *file, int line, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vsnprintf(log_buf, sizeof(log_buf), msg, ap);
	_printflevel("Panic", log_buf);
	va_end(ap);
}

noinline void _Error(const char *file, int line, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vsnprintf(log_buf, sizeof(log_buf), msg, ap);
	_printflevel("Error", log_buf);
	va_end(ap);
}
