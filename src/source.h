#ifndef TOF_MXREC_SOURCE_H
#define TOF_MXREC_SOURCE_H

#include "playlist.h"
#include "xmalloc.h"
#include <stddef.h>

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
	/**
	 *
	 */
	enum recomm_level {
		RECOMM_SIMPLE = 0,
		RECOMM_FULL = 1,
	} level;

} recomm_option;

// function filed

// remove relative resources here, release the memory by `source_free`
typedef void (*source_destroy)(void *sp);
/**
 * store recomm result in pointer p.
 * return 0 on success, else return a negative number when error.
 */
typedef int (*recomm_single_fp)(source *s, playentry *p, recomm_option opts);
/**
 * recomm lists of tracks with 'num' and store the result in pointer p.
 * return the real number of array p, else return a negative when error.
 */
typedef int (*recomm_multi_fp)(source *s, size_t num, playlist *p, recomm_option opts);

typedef struct source {
	source_destroy destroy;

	recomm_multi_fp rmp;

	recomm_single_fp rsp;
	/// security module
	/// some sources may restrict or even ban the access,
	/// this module is used for bypass such limitations.
	void *security;

	void *userdata;
} source;

static inline int _recomm_single(void *sp, playentry *p, recomm_option opts)
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
