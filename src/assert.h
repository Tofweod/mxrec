#ifndef TOF_MXREC_ASSERT_H
#define TOF_MXREC_ASSERT_H

#include "comm.h"

#ifndef static_assert
#define static_assert(expr, msg) extern char __static_assert_failure[(expr) ? 1 : -1]
#endif

#define assert(e) (likely((e)) ? (void)0 : (_Assert(#e, __FILE__, __LINE__), mxrec_unreachable()))
#define panic(...) _Panic(__FILE__, __LINE__, __VA_ARGS__), mxrec_unreachable()
#define error(...) _Error(__FILE__, __LINE__, __VA_ARGS__)

void _Assert(const char *e, const char *file, int line);
noreturn void _Panic(const char *file, int line, const char *msg, ...);
void _Error(const char *file, int line, const char *msg, ...);

#endif
