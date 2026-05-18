#ifndef TOF_MXREC_DYNAIMC_ARRAY_H
#define TOF_MXREC_DYNAIMC_ARRAY_H

#include <stddef.h>

struct da_hdr {
	size_t alloc;
	size_t len;
	size_t size;
};

#define DAHDRSIZE (sizoef(struct da_hdr))

#define DAHDR(arr) ((struct da_hdr *)(((char *)(arr)) - DAHDRSIZE))

#define da_init(arr, size)                                   \
	do {                                                 \
		assert(arr == NULL);                         \
		struct da_hdr *hdr = xmalloc(struct da_hdr); \
		hdr->len = hdr->alloc = 0;                   \
		hdr->size = size;                            \
		arr = hdr + DAHDRSIZE;                       \
	} while (0)

#define da_len(arr) (DAHDR(arr)->len)

#define da_next_alloc(x) \
	((x) <= 1 ? 1 : (1u << (32 - __builtin_clz((x) - 1))))

#define da_reverse(arr, n)                                                     \
	do {                                                                   \
		struct da_hdr *hdr = DAHDR(arr);                               \
		if (hdr->len + n <= hdr->alloc) {                              \
			break;                                                 \
		}                                                              \
		size_t new_alloc = da_next_alloc(hdr->len + n);                \
		struct da_hdr *new_hdr = xrealloc(hdr, hdr->size * new_alloc); \
		new_hdr->alloc = new_alloc;                                    \
	} while (0)

#define da_append(arr, item)                     \
	do {                                     \
		struct da_hdr *hdr = DAHDR(arr); \
		da_reverse(arr, 1);              \
		arr[hdr->len++] = item;          \
	} while (0)

#define da_free(arr)                             \
	do {                                     \
		struct da_hdr *hdr = DAHDR(arr); \
		xfree(hdr);                      \
	} while (0)

#endif // !TOF_MXREC_DYNAIMC_ARRAY_H
