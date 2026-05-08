#include "assert.h"
#include "comm.h"
#include <stdarg.h>
#include <stdio.h>

#define MAX_LOG_SIZE 256

noinline void _Assert(const char *estr, const char *file, int line)
{

	printf("[Assert failed]:%s in %s:%d\n", estr, file, line);
}

noinline void _Panic(const char *file, int line, const char *msg, ...)
{
	// TODO
	static char buf[MAX_LOG_SIZE];
	va_list ap;
	va_start(ap, msg);
	vsnprintf(buf, sizeof(buf), msg, ap);

	printf("[Panic]: trigged at %s:%d :%s\n", file, line, buf);
	va_end(ap);
}
