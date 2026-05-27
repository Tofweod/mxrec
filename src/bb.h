#ifndef TOF_MXREC_BUFFER_BUILDER_H
#define TOF_MXREC_BUFFER_BUILDER_H

#include <string.h>

#define __BB_TYPE(bbtype, buf_t)                                                                                       \
	typedef struct bbtype {                                                                                        \
		buf_t *buf;                                                                                            \
		size_t len;                                                                                            \
		size_t cap;                                                                                            \
	} bbtype;

#define __BB_INIT(SCOPE, bbtype, prefix, buf_t)                                                                        \
	SCOPE void prefix##_init(bbtype *bb, size_t cap)                                                               \
	{                                                                                                              \
		bb->cap = cap;                                                                                         \
		bb->len = 0;                                                                                           \
		bb->buf = xmalloc(cap);                                                                                \
	}

#define __BB_FREE(SCOPE, bbtype, prefix)                                                                               \
	SCOPE void prefix##_free(bbtype *bb)                                                                           \
	{                                                                                                              \
		if (!bb)                                                                                               \
			return;                                                                                        \
		xfree(bb->buf);                                                                                        \
	}

#define __BB_CLEAR(SCOPE, bbtype, prefix)                                                                              \
	SCOPE void prefix##_clear(bbtype *bb) { bb->len = 0; }

#define __BB_REVERSE(SCOPE, bbtype, prefix, buf_t)                                                                     \
	SCOPE void prefix##_reverse(bbtype *bb, size_t need)                                                           \
	{                                                                                                              \
		if (bb->len + need <= bb->cap)                                                                         \
			return;                                                                                        \
		size_t old_cap = bb->cap;                                                                              \
		assert(bb->len + need > bb->len);                                                                      \
		while ((bb->len + need) > bb->cap)                                                                     \
			bb->cap <<= 1;                                                                                 \
		assert(old_cap <= bb->cap);                                                                            \
		bb->buf = xrealloc(bb->buf, bb->cap);                                                                  \
	}

#define __BB_APPEND(SCOPE, bbtype, prefix, buf_t)                                                                      \
	SCOPE void prefix##_append(bbtype *bb, const void *src, size_t n)                                              \
	{                                                                                                              \
		prefix##_reverse(bb, n);                                                                               \
		memcpy(bb->buf + bb->len, src, n);                                                                     \
		bb->len += n;                                                                                          \
	}

#define BUFFERBUILDER_INIT(SCOPE, bbtype, prefix, buf_t)                                                               \
	__BB_TYPE(bbtype, buf_t);                                                                                      \
	__BB_INIT(SCOPE, bbtype, prefix, buf_t);                                                                       \
	__BB_FREE(SCOPE, bbtype, prefix);                                                                              \
	__BB_CLEAR(SCOPE, bbtype, prefix);                                                                             \
	__BB_REVERSE(SCOPE, bbtype, prefix, buf_t);                                                                    \
	__BB_APPEND(SCOPE, bbtype, prefix, buf_t);

#endif
