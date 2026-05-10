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
#define unused __attribute__((__unused__))
#define noinline __attribute__((noinline))
#define packed __attribute__((__packed__))
#define noreturn __attribute__((__noreturn__))
#elif
#define unused
#define noinline
#define packed
#endif

#endif
