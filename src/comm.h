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

#define mxrec_cleanup(tag, ret, val)                                                                                   \
	do {                                                                                                           \
		(ret) = (val);                                                                                         \
		goto tag;                                                                                              \
	} while (0)

// bit control
#define MXREC_BITSET(x, n) ((x) |= (1UL << (n)))
#define MXREC_BITCLEAR(x, n) ((x) &= ~(1UL << (n)))
#define MXREC_BITFLIP(x, n) ((x) ^= (1UL << (n)))
#define MXREC_BITTEST(x, n) ((x) & (1UL << (n)))

#if defined(__GNUC__) || defined(__clang__)
#define MXREC_BITCOUNT(x)                                                                                              \
	_Generic((x),                                                                                                  \
		unsigned char: __builtin_popcount(x),                                                                  \
		unsigned short: __builtin_popcount(x),                                                                 \
		unsigned int: __builtin_popcount(x),                                                                   \
		unsigned long: __builtin_popcountl(x),                                                                 \
		unsigned long long: __builtin_popcountll(x),                                                           \
		default: __mxrec_bitcount_fallback(x))
#else
#define MXREC_BITCOUNT(x) __mxrec_bitcount_fallback(x)
#endif

static inline int __mxrec_bitcount_fallback(unsigned long long v)
{
	v = v - ((v >> 1) & 0x5555555555555555ULL);
	v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
	v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
	return (v * 0x0101010101010101ULL) >> 56;
}

#endif
