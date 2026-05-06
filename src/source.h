#ifndef TOF_MXREC_SOURCE_H
#define TOF_MXREC_SOURCE_H

#include "xmalloc.h"
#include <stddef.h>

typedef struct playlist playlist;

/**
 * void* pointer 'sp' points to a concret source implement.
 * there is a source new function in each sourc implement's header file. Take
 * lastfm as example for the usage of source_init:
 * ```c
 * source *s;
 * if(lastfm_source_new(&s) < 0) {
 * 	// error
 * }
 * ```
 */
typedef struct source source;
typedef struct recomm_option {

} recomm_option;

// function filed

typedef void (*source_destroy)(void *sp);
typedef int (*recomm_single_fp)(source *s, playlist *p, recomm_option opts);
typedef int (*recomm_multi_fp)(source *s, size_t num, playlist *p, recomm_option opts);

typedef struct source {
	// remove relative resources here
	source_destroy destroy;

	recomm_multi_fp rmp;

	recomm_single_fp rsp;
	/// security module
	/// some sources may restrict or even ban the access,
	/// this module is used for bypass such limitations.
	void *security;

	void *userdata;
} source;

static inline int _recomm_single(void *sp, playlist *p, recomm_option opts)
{
	source *s = (source *)sp;
	return s->rsp(s, p, opts);
}
static inline int _recomm_multi(void *sp, size_t num, playlist *p, recomm_option opts)
{
	source *s = (source *)sp;
	return s->rmp(s, num, p, opts);
}

static inline void source_free(source *s)
{
	s->destroy(s);
	xfree(s);
}

static inline void source_setuserdata(source *s, void *data)
{
	s->userdata = data;
}

#define recomm_single(s, p, ...) _recomm_single((s), (p), (recomm_option){__VA_ARGS__})
#define recomm_multi(s, n, p, ...) _recomm_multi((s), (n), (p), (recomm_option){__VA_ARGS__})

#endif // !TOF_MXREC_SOURCE_H
