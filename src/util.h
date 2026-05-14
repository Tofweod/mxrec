#ifndef TOF_MXREC_UTIL_H
#define TOF_MXREC_UTIL_H

#ifndef static_assert
#define static_assert(expr, msg) extern char __static_assert_failure[(expr) ? 1 : -1]
#endif

// parse function
char *parseKVFormat(const char *src,...);

char *parseFormat(const char *fmt, ...);

#endif // !TOF_MXREC_UTIL_H
