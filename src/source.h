#ifndef TOF_MXREC_SOURCE_H
#define TOF_MXREC_SOURCE_H

#include "comm.h"
#include "playlist.h"
#include "xmalloc.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * void* pointer 'sp' points to a concret source implement.
 * there is a source new function in each sourc implement's header file. Take
 * lastfm as example for the usage of source_init:
 * ```c
 * source *s;
 * if(lastfm_source_new(&s,cfg) < 0) {
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

	bool use_security;
} recomm_option;

// function filed

// remove relative resources here, release the memory by `source_free`
typedef void (*source_destroy)(void *sp);
/**
 * store recomm result in pointer p.
 * return 0 on success, else return a negative number when error.
 */
typedef int (*recomm_single_fp)(source *s, playitem *p, recomm_option opts);
/**
 * recomm lists of tracks with 'num' and store the result in pointer p.
 * return the real number of array p, else return a negative when error.
 */
typedef int (*recomm_multi_fp)(source *s, size_t num, playlist *p, recomm_option opts);

typedef void (*security_handle)(source *s);

// TODO parameter optimization
typedef int (*before_recomm)(source *s, void *userdata);
typedef int (*after_recomm)(source *s, void *userdata);

typedef struct source {
	source_destroy destroy;

	recomm_multi_fp rmp;

	recomm_single_fp rsp;

	security_handle sh;
	/// security module
	/// some sources may restrict or even ban the access,
	/// this module is used for bypass such limitations.
	void *security;

	before_recomm br;
	after_recomm ar;

	void *userdata;
} source;

static inline void source_clearuserdata(source *s);

static inline int source_before_recomm(source *s, void *userdata)
{
	return s->br ? s->br(s, userdata) : 0;
}

static inline int source_after_recomm(source *s, void *userdata)
{
	return s->ar ? s->ar(s, userdata) : 0;
}

static inline int _recomm_single(void *sp, playitem *p, recomm_option opts)
{
	int ret;
	source *s = (source *)sp;
	if (source_before_recomm(s, s->userdata) < 0) {
		return -1;
	}
	source_clearuserdata(s);
	ret = s->rsp(s, p, opts);
	if (ret < 0)
		mxrec_cleanup(rs_cleanup, ret, ret);
	ret = source_after_recomm(s, s->userdata);
rs_cleanup:
	source_clearuserdata(s);
	return ret;
}

static inline int _recomm_multi(void *sp, size_t num, playlist *p, recomm_option opts)
{
	int ret;
	source *s = (source *)sp;
	if (source_before_recomm(s, s->userdata) < 0) {
		return -1;
	}
	source_clearuserdata(s);
	ret = s->rmp(s, num, p, opts);
	if (ret < 0)
		mxrec_cleanup(rm_cleanup, ret, ret);
	ret = source_after_recomm(s, s->userdata);
rm_cleanup:
	source_clearuserdata(s);
	return ret;
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

static inline void source_clearuserdata(source *s)
{
	s->userdata = NULL;
}

static inline void perform_security(void *sp)
{
	source *s = (source *)sp;
	s->sh(s);
}

#define recomm_single(s, p, ...) _recomm_single((s), (p), (recomm_option){__VA_ARGS__})
#define recomm_multi(s, n, p, ...) _recomm_multi((s), (n), (p), (recomm_option){__VA_ARGS__})

#endif // !TOF_MXREC_SOURCE_H
