#ifndef TOF_MXREC_SOURCE_H
#define TOF_MXREC_SOURCE_H

#include "assert.h"
#include "mxrec-error.h"
#include "playlist.h"
#include "xmalloc.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
struct source_result;

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

	mxrec_error *error;
	bool strict;
	bool use_security;
	bool progress_bar;

	// lastfm api
	struct {
		/**
		 * Maximum diffusion depth; zero disables expansion.
		 */
		uint64_t diffusion;
		uint64_t diff_size;
		uint64_t seed_count;
		uint64_t request_budget;
		uint64_t node_budget;

		double random_lambda;
		double diff_lambda;
		double score_beta;
	} lastfmapi_opts;

	// ncm
	struct {
		bool daily_recomm_fresh;
	} ncm_opts;

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
 * The public recomm_multi wrapper returns at most num unique songs, or a
 * negative number on error. Raw callbacks transfer ownership of *p (including
 * any partial result on failure) to the wrapper; leave *p NULL if none exists.
 */
typedef int (*recomm_multi_fp)(source *s, size_t num, playlist *p, recomm_option opts);

typedef void (*security_handle)(source *s);

typedef int (*before_recomm)(source *s, void *userdata);
typedef int (*after_recomm)(source *s, void *userdata);

typedef bool (*config_check)(source *s);
typedef int (*validate_options_fp)(const source *s, const recomm_option *opts, mxrec_error *error);

typedef void (*update_recomm)(void *update_entry, size_t cur, size_t total, const char *desc);
typedef void (*update_clear)(void *update_entry);

typedef struct source {
	const char *name;
	mxrec_error *attempt_error;
	source_destroy destroy;

	recomm_multi_fp rmp;

	recomm_single_fp rsp;

	security_handle sh;
	/// security module
	/// some sources may restrict or even ban the access,
	/// this module is used for bypass such limitations.
	void *security;

	config_check cc;
	/* Optional recommendation validation; never performs a fetch. */
	validate_options_fp validate_options;

	before_recomm br;
	after_recomm ar;

	void *update_entry;
	update_recomm ur;
	update_clear uc;

	void *userdata;

	// general settings
	/* Effective per-request timeout in ms; zero disables it. Must fit in long. */
	uint64_t timeout;
	uint64_t max_try;
	/* Preferred share when weighted merging is selected. */
	double merge_weight;
	/* A successful short recommendation may be followed by another batch. */
	bool refill_on_shortage;
	/* Optional source-specific ordering before merge iterators are created.
	 * A NULL result discards private collection state without touching items. */
	void (*finalize_result)(source *s, struct source_result *result);
} source;

static inline void source_clearuserdata(source *s);

/* Validate the effective source settings and this recommendation's options.
 * Sessions call once at initialization; standalone wrappers call once per use. */
static inline int source_validate_options(const source *s, recomm_option opts)
{
	mxrec_error local = {0};
	mxrec_error *err = opts.error ? opts.error : &local;
	size_t before = err->count;
	int ret = 0;
	if (!s) {
		mxrec_error_add(err, "source configuration", "source is NULL");
		ret = -1;
	} else {
		if (s->timeout > LONG_MAX) {
			mxrec_error_add(err, "source configuration", "effective timeout must not exceed %ld ms",
					LONG_MAX);
			ret = -1;
		}
		if (s->validate_options && s->validate_options(s, &opts, err) < 0)
			ret = -1;
	}
	if (ret < 0 && err->count == before)
		mxrec_error_add(err, "source configuration", "recommendation options are invalid");
	if (ret < 0 && !opts.error)
		mxrec_error_print(stderr, err, s ? s->name : NULL, 0, 0, ret, false);
	return ret;
}

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
	if (source_validate_options(s, opts) < 0)
		return -1;
	return s->rsp(s, p, opts);
}

static inline int _recomm_multi(void *sp, size_t num, playlist *p, recomm_option opts)
{
	source *s = (source *)sp;
	int ret;
	assert(s->rmp);
	if (!p)
		return -1;
	*p = NULL;
	if (num > INT_MAX)
		return -1;
	if (!num)
		return 0;
	if (s->finalize_result)
		s->finalize_result(s, NULL);
	if (source_validate_options(s, opts) < 0)
		return -1;
	ret = s->rmp(s, num, p, opts);
	if (s->finalize_result)
		s->finalize_result(s, NULL);
	if (ret >= 0)
		ret = playlist_unique_owned(p, num, opts.strict);
	if (ret < 0) {
		playlist_free(*p);
		*p = NULL;
	}
	return ret;
}

/* One raw batch using prevalidated options. The caller owns any returned
 * partial playlist, even on failure. Sessions use this for retries. */
static inline int source_fetch_batch(source *s, size_t num, playlist *p, recomm_option opts)
{
	if (!s || !s->rmp || !p || num > INT_MAX)
		return -1;
	*p = NULL;
	if (!num)
		return 0;
	size_t before = opts.error ? opts.error->count : 0;
	s->attempt_error = opts.error;
	int ret = s->rmp(s, num, p, opts);
	s->attempt_error = NULL;
	if (ret < 0 && opts.error && opts.error->count == before)
		mxrec_error_add(opts.error, "recommendation", "source returned error %d", ret);
	return ret;
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
