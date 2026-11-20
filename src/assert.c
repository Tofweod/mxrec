#include "assert.h"
#include "comm.h"
#include <stdarg.h>
#include <stdio.h>
#define MAX_LOG_SIZE 1024
mxrec_noinline void _Assert(const char *estr, const char *file, int line)
{
	flockfile(stderr);
	fprintf(stderr, "[Assert]: %s failed in %s:%d\n", estr, file, line);
	funlockfile(stderr);
	abort();
}
mxrec_noreturn mxrec_noinline void _Panic(const char *file, int line, const char *msg, ...)
{
	char buf[MAX_LOG_SIZE];
	va_list ap;
	va_start(ap, msg);
	vsnprintf(buf, sizeof(buf), msg, ap);
	va_end(ap);
	flockfile(stderr);
	fprintf(stderr, "[Panic]: %s in %s:%d\n", buf, file, line);
	funlockfile(stderr);
	abort();
}
mxrec_noinline void _Error(const char *file, int line, const char *msg, ...)
{
	char buf[MAX_LOG_SIZE];
	va_list ap;
	va_start(ap, msg);
	vsnprintf(buf, sizeof(buf), msg, ap);
	va_end(ap);
	flockfile(stderr);
	fprintf(stderr, "[Error]: %s in %s:%d\n", buf, file, line);
	funlockfile(stderr);
}
