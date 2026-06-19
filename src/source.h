#ifndef TOF_MXREC_SOURCE_H
#define TOF_MXREC_SOURCE_H

#include "assert.h"
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
	 * Only support simple now. For most sources with return imcompleted
	 * results for speed, FULL level requires multi-time queries
	 * which may be implemented in the future.
	 */
	enum recomm_level {
		RECOMM_SIMPLE = 0,
		RECOMM_FULL = 1,
	} level;

	bool strict;
	bool use_security;

	union {
		// lastfm api
		struct {
			/**
			 * Diffusion should be positive, representing the maximum diffusion level.
			 */
			unsigned diffusion;
			unsigned diff_size;

			double random_lambda;
			double diff_lambda;
			double score_beta;
		} lastfmapi_opts;

		// ncm
		struct {
			bool daily_recomm_fresh;
		} ncm_opts;
	};

} recomm_option;

// function field

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

typedef int (*before_recomm)(source *s, void *userdata);
typedef int (*after_recomm)(source *s, void *userdata);

typedef bool (*config_check)(source *s);

typedef struct source {
	const char* name;
	source_destroy destroy;

	recomm_multi_fp rmp;

	recomm_single_fp rsp;

	security_handle sh;
	/// security module
	/// some sources may restrict or even ban the access,
	/// this module is used for bypass such limitations.
	void *security;

	config_check cc;

	before_recomm br;
	after_recomm ar;

	void *userdata;
} source;

static inline void source_clearuserdata(source *s);

static inline int source_before_recomm(source *s)
{
	int ret = 0;
	if (s->br) {
		ret = s->br(s, s->userdata);
		source_clearuserdata(s);
	}
	return ret;
}

static inline int source_after_recomm(source *s)
{
	int ret = 0;
	if (s->ar) {
		ret = s->ar(s, s->userdata);
		source_clearuserdata(s);
	}
	return ret;
}

static inline int _recomm_single(void *sp, playitem *p, recomm_option opts)
{
	source *s = (source *)sp;
	assert(s->rsp);
	return s->rsp(s, p, opts);
}

static inline int _recomm_multi(void *sp, size_t num, playlist *p, recomm_option opts)
{
	source *s = (source *)sp;
	assert(s->rmp);
	return s->rmp(s, num, p, opts);
}

static inline void source_free(source *s)
{
	s->destroy(s);
	xfree(s);
}

static inline void source_setuserdata(source *s, void *data) { s->userdata = data; }

static inline void source_clearuserdata(source *s) { s->userdata = NULL; }

static inline void source_perform_security(void *sp)
{
	source *s = (source *)sp;
	if (s->sh)
		s->sh(s);
}

static inline bool source_check(void *sp)
{
	source *s = (source *)sp;
	if (s->cc)
		return s->cc(s);
	return true;
}

#define recomm_single(s, p, ...) _recomm_single((s), (p), (recomm_option){__VA_ARGS__})
#define recomm_multi(s, n, p, ...) _recomm_multi((s), (n), (p), (recomm_option){__VA_ARGS__})

#endif // !TOF_MXREC_SOURCE_H
