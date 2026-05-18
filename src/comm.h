#ifndef TOF_MXREC_COMM_H
#define TOF_MXREC_COMM_H

#include <stdbool.h>
#include <stdlib.h>

#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define mxrec_unreachable __builtin_unreachable
#else
#define mxrec_unreachable abort
#endif

#if __GNUC__ >= 3
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#ifdef __GNUC__
#define mxrec_unused __attribute__((__unused__))
#define mxrec_noinline __attribute__((noinline))
#define mxrec_packed __attribute__((__packed__))
#define mxrec_noreturn __attribute__((__noreturn__))
#elif
#define mxrec_unused
#define mxrec_noinline
#define mxrec_packed
#endif

#define mxrec_cleanup(tag, ret, val) \
	do {                         \
		(ret) = (val);       \
		goto tag;            \
	} while (0)

#endif
