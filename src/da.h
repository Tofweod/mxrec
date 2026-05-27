#ifndef TOF_MXREC_DYNAIMC_ARRAY_H
#define TOF_MXREC_DYNAIMC_ARRAY_H

#include <stddef.h>

struct da_hdr {
	size_t alloc;
	size_t len;
	size_t size;
};

#define DAHDRSIZE (sizeof(struct da_hdr))

#define DAHDR(arr) ((struct da_hdr *)(((char *)(arr)) - DAHDRSIZE))

#define da_init(arr, _size)                                                                                            \
	do {                                                                                                           \
		assert(arr == NULL);                                                                                   \
		struct da_hdr *hdr = xmalloc(DAHDRSIZE);                                                               \
		hdr->len = hdr->alloc = 0;                                                                             \
		hdr->size = _size;                                                                                     \
		arr = (void *)((char *)hdr + DAHDRSIZE);                                                               \
	} while (0)

#define da_len(arr) (DAHDR(arr)->len)

#define da_next_alloc(x) ((x) <= 1 ? 1 : (1u << (32 - __builtin_clz((x) - 1))))

#define da_reverse(arr, n)                                                                                             \
	({                                                                                                             \
		struct da_hdr *hdr = DAHDR(arr);                                                                       \
		struct da_hdr *new_hdr = hdr;                                                                          \
		do {                                                                                                   \
			if (hdr->len + n <= hdr->alloc) {                                                              \
				break;                                                                                 \
			}                                                                                              \
			size_t new_alloc = da_next_alloc(hdr->len + n);                                                \
			new_hdr = xrealloc(hdr, DAHDRSIZE + hdr->size * new_alloc);                                    \
			new_hdr->alloc = new_alloc;                                                                    \
		} while (0);                                                                                           \
		(void *)((char *)new_hdr + DAHDRSIZE);                                                                 \
	})

#define da_append(arr, item)                                                                                           \
	do {                                                                                                           \
		arr = da_reverse(arr, 1);                                                                              \
		struct da_hdr *hdr = DAHDR(arr);                                                                       \
		arr[hdr->len++] = item;                                                                                \
	} while (0)

#define da_append_arr(dest, src, n)                                                                                    \
	do {                                                                                                           \
		dest = da_reverse(dest, n);                                                                            \
		struct da_hdr *hdr = DAHDR(dest);                                                                      \
		memcpy(dest + hdr->len, src, n * hdr->size);                                                           \
		hdr->len += n;                                                                                         \
	} while (0)

#define da_free(arr)                                                                                                   \
	do {                                                                                                           \
		struct da_hdr *hdr = DAHDR(arr);                                                                       \
		xfree(hdr);                                                                                            \
	} while (0)

#endif // !TOF_MXREC_DYNAIMC_ARRAY_H
