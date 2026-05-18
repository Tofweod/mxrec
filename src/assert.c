#include "assert.h"
#include "comm.h"
#include <stdarg.h>
#include <stdio.h>

#define MAX_LOG_SIZE 1024

static char log_buf[MAX_LOG_SIZE];

static inline void _printflevel(const char *level, const char *msg)
{
	fprintf(stderr, "[%s]: %s\n", level, msg);
}

mxrec_noinline void _Assert(const char *estr, const char *file, int line)
{
	snprintf(log_buf, sizeof(log_buf), "%s failed in %s:%d", estr, file, line);
	_printflevel("Assert", log_buf);
}

mxrec_noreturn mxrec_noinline void _Panic(const char *file, int line, const char *msg, ...)
{
	static char buf[MAX_LOG_SIZE / 2];
	va_list ap;
	va_start(ap, msg);
	vsnprintf(buf, sizeof(buf), msg, ap);
	snprintf(log_buf, sizeof(log_buf), "%s in %s:%d", buf, file, line);
	_printflevel("Panic", log_buf);
	va_end(ap);
	abort();
}

mxrec_noinline void _Error(const char *file, int line, const char *msg, ...)
{
	static char buf[MAX_LOG_SIZE / 2];
	va_list ap;
	va_start(ap, msg);
	vsnprintf(buf, sizeof(buf), msg, ap);
	snprintf(log_buf, sizeof(log_buf), "%s in %s:%d", buf, file, line);
	_printflevel("Error", log_buf);
	va_end(ap);
}
