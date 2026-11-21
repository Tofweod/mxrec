#include "lastfm-api.h"
#include "artist.h"
#include "bb.h"
#include "comm.h"
#include "config.h"
#include "curl-impersonate.h"
#include "da.h"
#include "json.h"
#include "lastfm-comm.h"
#include "monotonic.h"
#include "playlist-viewer.h"
#include "playlist.h"
#include "random.h"
#include "source.h"
#include "source/comm-source.h"
#include "track-key.h"
#include "track.h"
#include "u8string.h"
#include "uthash.h"
#include "utils/string.h"
#include "utils/time.h"
#include "xmalloc.h"
#include "yyjson/src/yyjson.h"
#include <curl/curl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define LASTFMAPI_DECL static

#define JSON_ERR_HEAD "LASTFMAPI"

// methods of api
#define LASTFMAPI_FORMAT "json"
#define LASTFMAPI_USER_GETRECENTTRACKS "user.getRecentTracks"
#define LASTFMAPI_TRACK_GETSIMILAR "track.getSimilar"
#define LASTFMAPI_ARTIST_GETSIMILAR "artist.getSimilar"
#define LASTFMAPI_ARTIST_GETTOPTRACKS "artist.getTopTracks"

// curlbuf
#define CURLBUF_DEFAULT_CAP (1024)
BUFFERBUILDER_INIT(LASTFMAPI_DECL, curlbuf, curlbuf, void);

LASTFMAPI_DECL
size_t lastfmapi_write_callback(char *ptr, const size_t size, const size_t nmemb, curlbuf *cb)
{
	const size_t real = size * nmemb;
	curlbuf_append(cb, ptr, real);

	return real;
}

#define FUNCTION_FIELD(retype, name, ...) LASTFMAPI_DECL retype lastfmapi_##name(__VA_ARGS__);

FUNCTION_FIELD_LIST

#undef FUNCTION_FIELD

LASTFMAPI_DECL
void lastfmapi_finalize_result(source *s, struct source_result *result);
LASTFMAPI_DECL
int lastfmapi_validate_options(const source *s, const recomm_option *opts, mxrec_error *error);

typedef struct lastfmapi_artist {
	u8s name;
	char *mbid;
	double match;
	unsigned diffusion;
} la_artist;

LASTFMAPI_DECL
void la_artist_init(la_artist *lar, const char *name, const char *mbid, double match)
{
	if (lar == NULL)
		return;
	lar->name = u8snew(name);
	lar->mbid = xstrdup(mbid);
	lar->match = match;
}

LASTFMAPI_DECL
void la_artist_cleanup(la_artist *lar)
{
	if (lar == NULL)
		return;
	u8sfree(lar->name);
	xfree(lar->mbid);
}

/* One pool is owned by one recomm_multi invocation. Nodes borrow immutable
 * canonical keys; the pool outlives history and all diffusion path nodes. */
typedef struct la_identity {
	track_key key;
	UT_hash_handle hh;
} la_identity;

typedef struct la_identity_pool {
	la_identity *head;
} la_identity_pool;

LASTFMAPI_DECL
const track_key *la_identity_intern(la_identity_pool *pool, const char *name, const char *artist)
{
	assert(pool);
	track_key key = {0};
	track_key_text title = {name, name ? strlen(name) : 0};
	track_key_text ar = {artist, artist ? strlen(artist) : 0};
	if (track_key_build_fields(title, &ar, 1, &key) < 0)
		return NULL;

	la_identity *identity;
	HASH_FIND_BYHASHVALUE(hh, pool->head, key.data, key.len, key.hash, identity);
	if (identity) {
		track_key_free(&key);
		return &identity->key;
	}
	identity = xmalloc(sizeof(*identity));
	identity->key = key;
	HASH_ADD_KEYPTR_BYHASHVALUE(hh, pool->head, identity->key.data, identity->key.len, identity->key.hash,
				    identity);
	return &identity->key;
}

LASTFMAPI_DECL
void la_identity_pool_free(la_identity_pool *pool)
{
	la_identity *identity, *tmp;
	HASH_ITER(hh, pool->head, identity, tmp)
	{
		HASH_DEL(pool->head, identity);
		track_key_free(&identity->key);
		xfree(identity);
	}
}

typedef struct lastfmapi_track la_track;
struct lastfmapi_track {
	struct tr_key_t {
		u8s name;
		u8s artist;
		// mbid can be NULL
		char *ar_mbid;
		char *tr_mbid;
		const track_key *identity; /* Borrowed from this attempt's identity pool. */
	} key;
	// used for tracing diffusion
	double score;
	time_t ruts;
	la_track *parent;
	int diffusion;
	double match;
	UT_hash_handle hh;
};

LASTFMAPI_DECL
mxrec_unused void la_track_dump(FILE *fp, la_track *ltr)
{
	fprintf(fp,
		"name:%s\n"
		"\tartist:%s\n"
		"\truts:%zu\n"
		"\tdiffusion:%d\n",
		ltr->key.name, ltr->key.artist, ltr->ruts, ltr->diffusion);
	if (ltr->key.tr_mbid)
		fprintf(fp, "\ttr_mbid:%s\n", ltr->key.tr_mbid);
	if (ltr->key.ar_mbid)
		fprintf(fp, "\tar_mbid:%s\n", ltr->key.ar_mbid);
	fprintf(fp,
		"\tmatch:%lf\n"
		"\tscore:%lf\n",
		ltr->match, ltr->score);
}

LASTFMAPI_DECL
la_track *la_track_new(const char *name, const char *artist, const char *tr_mbid, const char *ar_mbid,
		       la_identity_pool *identities)
{
	la_track *ltr;
	ltr = xmalloc(sizeof(*ltr));
	memset(ltr, 0, sizeof(*ltr));
	ltr->key.name = u8snew(name);
	ltr->key.artist = u8snew(artist);
	ltr->key.tr_mbid = xstrdup(tr_mbid);
	ltr->key.ar_mbid = xstrdup(ar_mbid);
	ltr->key.identity = la_identity_intern(identities, name, artist);

	return ltr;
}

LASTFMAPI_DECL
void la_track_free(la_track *ltr)
{
	if (ltr == NULL)
		return;
	u8sfree(ltr->key.name);
	u8sfree(ltr->key.artist);
	xfree(ltr->key.ar_mbid);
	xfree(ltr->key.tr_mbid);
	xfree(ltr);
}

/* Hash keys are owned canonical bytes, never pointer-containing structs or MBIDs. */
LASTFMAPI_DECL
int la_track_cmp(const struct tr_key_t *a, const struct tr_key_t *b)
{
	const track_key *ka = a->identity, *kb = b->identity;
	if (!ka || !kb)
		return (ka != NULL) - (kb != NULL);
	size_t n = ka->len < kb->len ? ka->len : kb->len;
	int cmp = memcmp(ka->data, kb->data, n);
	return cmp ? cmp : (ka->len > kb->len) - (ka->len < kb->len);
}

LASTFMAPI_DECL
int la_track_map_cmp(const la_track *a, const la_track *b)
{
	if (a->ruts == b->ruts)
		return la_track_cmp(&a->key, &b->key);
	return a->ruts < b->ruts ? -1 : 1;
}

LASTFMAPI_DECL
void la_track_map_cleanup(la_track *hm)
{
	if (hm == NULL)
		return;
	la_track *cur, *tmp;
#ifdef LASTFMAPI_DEBUG
	fprintf(stderr, "recent hash map is\n");
#endif
	HASH_ITER(hh, hm, cur, tmp)
	{
#ifdef LASTFMAPI_DEBUG
		la_track_dump(stderr, cur);
#endif
		HASH_DELETE(hh, hm, cur);
		la_track_free(cur);
	}
}

LASTFMAPI_DECL
int lastfmapi_latrack2playitem(la_track *ltr, playitem *pi)
{
	track *tr;
	artist **ars;
	size_t ar_size;
	playitem_init(pi);

	// build track
	// build artists
	ar_size = 1;
	ars = xmalloc(sizeof(*ars) * ar_size);
	ars[0] = artist_new((const char *)ltr->key.artist, 0, NULL);
	tr = track_new((const char *)ltr->key.name, NULL, ar_size, ars, 0, 0);
	pi->tr = tr;

	// add url
	return 0;
}

/* Candidates are already ranked. Only accepted songs need output objects. */
LASTFMAPI_DECL
int lastfmapi_select_unique(la_track *const *candidates, size_t count, size_t target, playlist *p_ref, bool strict)
{
	track_key_set seen = {0};
	playlist pl = NULL;
	int ret = -1;
	*p_ref = NULL;
	da_init(pl, sizeof(playitem));
	for (size_t i = 0; i < count && da_len(pl) < target; i++) {
		la_track *candidate = candidates[i];
		if (!candidate->key.identity) {
			if (strict)
				goto cleanup;
			continue;
		}
		bool added = track_key_set_add(&seen, candidate->key.identity) != 0;
		if (added) {
			playitem pi;
			lastfmapi_latrack2playitem(candidate, &pi);
			da_append(pl, pi);
		}
	}
	ret = (int)da_len(pl);
	*p_ref = pl;
	pl = NULL;
cleanup:
	track_key_set_free(&seen);
	playlist_free(pl);
	return ret;
}

// core: diffusion recommendation
typedef enum lastfm_diffusion_strategy {
	LASTFMAPI_DIFFUSION_ERR = 0,
	LASTFMAPI_DIFFUSION_BFS = 1,
	LASTFMAPI_DIFFUSION_DFS = 1 << 1,
	LASTFMAPI_DIFFUSION_TOPN = 1 << 2,
	LASTFMAPI_DIFFUSION_RAMSAMPLE = 1 << 3,
} lastfmapi_diffusion_strategy;

// data structure of dfs and bfs strategy diffusion
typedef struct df_deque df_deque;
struct df_deque {
	la_track **ltrs;
	size_t head;
	size_t tail;
	size_t size;
	size_t cap;
	void (*push)(df_deque *, la_track *);
	la_track *(*pop)(df_deque *);
};

LASTFMAPI_DECL
void df_deque_init(df_deque *dq, size_t cap)
{
	dq->head = dq->tail = dq->size = 0;
	dq->cap = cap;

	dq->ltrs = xmalloc(sizeof(*dq->ltrs) * cap);
}

LASTFMAPI_DECL
void df_deque_free(df_deque *dq)
{
	xfree(dq->ltrs);
	memset(dq, 0, sizeof(*dq));
}

LASTFMAPI_DECL
inline bool df_deque_empty(df_deque *dq) { return dq->size == 0; }

LASTFMAPI_DECL
inline size_t df_deque_next(df_deque *dq, size_t i) { return (i + 1) % dq->cap; }

LASTFMAPI_DECL
inline size_t df_deque_prev(df_deque *dq, size_t i) { return (i + dq->cap - 1) % dq->cap; }

LASTFMAPI_DECL
void df_deque_expand(df_deque *dq)
{
	size_t i, new_cap = dq->cap ? dq->cap * 2 : 8;
	la_track **new_ltrs = xmalloc(sizeof(*new_ltrs) * new_cap);

	for (i = 0; i < dq->size; ++i) {
		new_ltrs[i] = dq->ltrs[(dq->head + i) % dq->cap];
	}
	xfree(dq->ltrs);
	dq->ltrs = new_ltrs;
	dq->cap = new_cap;
	dq->head = 0;
	dq->tail = dq->size;
}

LASTFMAPI_DECL
void df_deque_push_back(df_deque *dq, la_track *ltr)
{
	if (dq->size == dq->cap)
		df_deque_expand(dq);

	dq->ltrs[dq->tail] = ltr;
	dq->tail = df_deque_next(dq, dq->tail);
	++dq->size;
}

LASTFMAPI_DECL
la_track *df_deque_pop_front(df_deque *dq)
{
	if (df_deque_empty(dq))
		return NULL;

	la_track *ltr = dq->ltrs[dq->head];
	dq->head = df_deque_next(dq, dq->head);
	--dq->size;
	return ltr;
}

LASTFMAPI_DECL
la_track *df_deque_pop_back(df_deque *dq)
{
	if (df_deque_empty(dq))
		return NULL;
	dq->tail = df_deque_prev(dq, dq->tail);
	la_track *ltr = dq->ltrs[dq->tail];
	--dq->size;
	return ltr;
}

LASTFMAPI_DECL
df_deque df_stack = {
	.pop = df_deque_pop_back,
	.push = df_deque_push_back,
};

LASTFMAPI_DECL
df_deque df_queue = {
	.pop = df_deque_pop_front,
	.push = df_deque_push_back,
};

LASTFMAPI_DECL
lastfmapi_diffusion_strategy str2strategy(const char *strategy, const char *sample)
{
	lastfmapi_diffusion_strategy traversal, sampling;
	if (strategy && strcmp(strategy, "bfs") == 0)
		traversal = LASTFMAPI_DIFFUSION_BFS;
	else if (strategy && strcmp(strategy, "dfs") == 0)
		traversal = LASTFMAPI_DIFFUSION_DFS;
	else {
		error("lastfm.api:strategy must be dfs or bfs (got %s)", strategy ? strategy : "NULL");
		return LASTFMAPI_DIFFUSION_ERR;
	}
	if (sample && strcmp(sample, "topn") == 0)
		sampling = LASTFMAPI_DIFFUSION_TOPN;
	else if (sample && strcmp(sample, "random") == 0)
		sampling = LASTFMAPI_DIFFUSION_RAMSAMPLE;
	else {
		error("lastfm.api:sample must be topn or random (got %s)", sample ? sample : "NULL");
		return LASTFMAPI_DIFFUSION_ERR;
	}
	return traversal | sampling;
}

LASTFMAPI_DECL
bool lastfmapi_strategy_valid(lastfmapi_diffusion_strategy strategy)
{
	unsigned traversal = strategy & (LASTFMAPI_DIFFUSION_BFS | LASTFMAPI_DIFFUSION_DFS);
	unsigned sampling = strategy & (LASTFMAPI_DIFFUSION_TOPN | LASTFMAPI_DIFFUSION_RAMSAMPLE);
	return (traversal == LASTFMAPI_DIFFUSION_BFS || traversal == LASTFMAPI_DIFFUSION_DFS) &&
	       (sampling == LASTFMAPI_DIFFUSION_TOPN || sampling == LASTFMAPI_DIFFUSION_RAMSAMPLE) &&
	       strategy == (traversal | sampling);
}

LASTFMAPI_DECL
int la_track_score_cmp(const void *a, const void *b)
{
	const la_track *la = *(const la_track *const *)a;
	const la_track *lb = *(const la_track *const *)b;
	if (la->score == lb->score)
		return la_track_cmp(&la->key, &lb->key);
	return la->score > lb->score ? -1 : 1;
}

typedef struct seed_rank {
	size_t index;
	double priority;
} seed_rank;

LASTFMAPI_DECL
int seed_rank_cmp(const void *pa, const void *pb)
{
	const seed_rank *a = pa, *b = pb;
	if (a->priority != b->priority)
		return a->priority < b->priority ? -1 : 1;
	return (a->index > b->index) - (a->index < b->index);
}

LASTFMAPI_DECL
int seed_index_cmp(const void *pa, const void *pb)
{
	size_t a = *(const size_t *)pa, b = *(const size_t *)pb;
	return (a > b) - (a < b);
}

/* Exponential races give weighted sampling without replacement. rank=0 is
 * newest; its weight is lambda times the oldest weight. Sort selected indices
 * afterward so the history map can still be consumed in one forward pass. */
LASTFMAPI_DECL
int _lastfmapi_recomm_random_sample(size_t len, size_t *idxs, size_t lb, size_t ub, double lambda)
{
	if (!len || !idxs || ub < lb || ub - lb == SIZE_MAX || !isfinite(lambda) || lambda <= 0)
		return 0;
	size_t count = ub - lb + 1;
	len = len < count ? len : count;
	if (len > INT_MAX || count > SIZE_MAX / sizeof(seed_rank))
		return 0;
	if (len == count) {
		for (size_t i = 0; i < len; i++)
			idxs[i] = lb + i;
		return (int)len;
	}
	seed_rank *ranks = xmalloc(count * sizeof(*ranks));
	for (size_t i = 0; i < count; i++) {
		double u;
		if (uniform01(&u) != 0) {
			xfree(ranks);
			return 0;
		}
		/* uniform01 includes zero. Work in log space to keep extreme
		 * positive lambda values and rare random endpoints finite. */
		u = fmax(DBL_MIN, fmin(u, nextafter(1.0, 0.0)));
		ranks[i].index = lb + i;
		ranks[i].priority = log(-log(u)) + log(lambda) * ((double)i / (count - 1));
	}
	qsort(ranks, count, sizeof(*ranks), seed_rank_cmp);
	for (size_t i = 0; i < len; i++)
		idxs[i] = ranks[i].index;
	qsort(idxs, len, sizeof(*idxs), seed_index_cmp);
	xfree(ranks);
	return (int)len;
}

/* Own every generated node, including history-only intermediate nodes. Parent
 * references remain valid until all paths have been scored and materialized. */
typedef struct diffusion_nodes {
	la_track **owned;
	bool exhausted;
} diffusion_nodes;

LASTFMAPI_DECL
size_t _lastfmapi_get_track_similar(la_track *ltr, unsigned diff_size, la_track ***res, lastfmapi_source *ls,
				    recomm_option opts, la_identity_pool *identities);
LASTFMAPI_DECL
size_t _lastfmapi_diffusion_artist(la_track *info, int diffusion, unsigned diff, la_track ***res, lastfmapi_source *ls,
				   recomm_option opts, la_identity_pool *identities);

/* Each seed keeps its own stack/queue between round-robin turns. */
typedef struct diffusion_seed {
	df_deque work;
	bool started;
} diffusion_seed;

LASTFMAPI_DECL
void _lastfmapi_diffusion_step(df_deque *work, int diffusion, const la_track *map, la_track ***res,
			       lastfmapi_source *ls, recomm_option opts, diffusion_nodes *nodes,
			       la_identity_pool *identities);
LASTFMAPI_DECL
int _lastfmapi_latracks_score(la_track **tracks, size_t len, double lambda, time_t period, double beta);

LASTFMAPI_DECL
int _lastfmapi_diffusion_core(la_track **src, size_t src_size, lastfmapi_diffusion_strategy strategy, int diffusion,
			      time_t period, la_track *map, playlist *p_ref, lastfmapi_source *ls, recomm_option opts,
			      la_identity_pool *identities);

LASTFMAPI_DECL
int lastfmapi_diffusion_recomm(lastfmapi_source *ls, recomm_option opts, la_track *map, playlist *p, time_t period,
			       la_identity_pool *identities);

/* Similarity is returned as either a JSON number or a decimal string. Invalid
 * similarities must not silently turn a whole path into NaN or infinity. */
LASTFMAPI_DECL
bool lastfmapi_match(yyjson_val *value, double *match)
{
	if (yyjson_is_num(value))
		*match = yyjson_get_num(value);
	else {
		const char *text = yyjson_get_str(value);
		if (!text || !string2d(text, strlen(text), match))
			return false;
	}
	return isfinite(*match) && *match >= 0 && *match <= 1;
}

// la_track json parsing
LASTFMAPI_DECL
la_track *lastfmapi_json2recent_latrack(yyjson_val *val, bool strict, time_t now, mxrec_error *err,
					la_identity_pool *identities)
{
	la_track *ltr;
	yyjson_val *jname, *jar;
	const char *name, *ar, *tr_mbid, *ar_mbid, *uts_s;
	unsigned long long uts;

	jname = yyjson_obj_get(val, "name");
	name = yyjson_get_str(jname);
	if (name == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	jar = yyjson_obj_get(val, "artist");
	ar = yyjson_get_str(yyjson_obj_get(jar, "#text"));
	if (ar == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	tr_mbid = yyjson_get_str(yyjson_obj_get(val, "mbid"));
	ar_mbid = yyjson_get_str(yyjson_obj_get(jar, "mbid"));

	uts_s = yyjson_get_str(yyjson_obj_get(yyjson_obj_get(val, "date"), "uts"));
	if (uts_s == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	if (!string2ull(uts_s, &uts))
		mxrec_cleanup(cleanup, ltr, 0);

	ltr = la_track_new(name, ar, tr_mbid, ar_mbid, identities);
	assert(now >= uts);
	ltr->ruts = now - uts;
	if (!ltr->key.identity) {
		la_track_free(ltr);
		ltr = NULL;
	}
cleanup:
	return ltr;
}

LASTFMAPI_DECL
int lastfmapi_jsonbuf2recent_latrack_map(curlbuf *buf, bool strict, la_track **map, mxrec_error *jerr,
					 la_identity_pool *identities)
{
	int ret;
	yyjson_read_err err;
	yyjson_doc *doc;
	yyjson_val *root, *tracks, *track;
	yyjson_arr_iter it;
	la_track *hm, *ltr;
	time_t now;
	size_t m_size = 0;
	hm = NULL;

	doc = yyjson_read_opts(buf->buf, buf->len, 0, NULL, &err);
	if (doc == NULL) {
		write_jsonerr(jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n", JSON_ERR_HEAD,
			      err.msg, err.code, err.pos);
		mxrec_cleanup(cleanup, ret, -1);
	}

	root = yyjson_doc_get_root(doc);
	tracks = yyjson_obj_get(yyjson_obj_get(root, "recenttracks"), "track");
	if (!yyjson_is_arr(tracks)) {
		write_jsonerr(jerr, "[%s]: failed to parse field \".recenttracks.track\" into array", JSON_ERR_HEAD);
		mxrec_cleanup(cleanup, ret, -1);
	}

	now = mxrec_now();
	it = yyjson_arr_iter_with(tracks);
	while ((track = yyjson_arr_iter_next(&it))) {
		la_track *find;
		ltr = lastfmapi_json2recent_latrack(track, strict, now, jerr, identities);
		if (ltr == NULL) {
			if (strict) {
				write_jsonerr(jerr, "[%s]: failed to get track", JSON_ERR_HEAD);
				mxrec_cleanup(cleanup, ret, -1);
			}
			continue;
		}
		HASH_FIND_BYHASHVALUE(hh, hm, ltr->key.identity->data, ltr->key.identity->len, ltr->key.identity->hash,
				      find);
		if (find != NULL) {
			if (ltr->ruts < find->ruts)
				find->ruts = ltr->ruts;
			la_track_free(ltr);
			continue;
		}
		HASH_ADD_KEYPTR_BYHASHVALUE(hh, hm, ltr->key.identity->data, ltr->key.identity->len,
					    ltr->key.identity->hash, ltr);
		++m_size;
	}
	// sort hash map with ruts firstly. (refer to la_track_map_cmp)
	HASH_SRT(hh, hm, la_track_map_cmp);

	ret = m_size;
cleanup:
	yyjson_doc_free(doc);
	*map = hm;
	return ret;
}

static source __lastfmapi_source = {
	.name = "lastfmapi",
	.refill_on_shortage = false,
	.finalize_result = lastfmapi_finalize_result,
	.destroy = lastfmapi_source_destroy,
	.rsp = lastfmapi_recomm_single,
	.rmp = lastfmapi_recomm_multi,
	.sh = lastfmapi_security_handle,
	.cc = lastfmapi_config_check,
	.validate_options = lastfmapi_validate_options,
	.security = &__lastfm_security,
};

typedef struct la_query_cache {
	char *url;
	char *data;
	size_t len;
	UT_hash_handle hh;
} la_query_cache;

/* Private collection state across attempts. Only compact ranking records
 * survive each attempt; diffusion nodes and their identity pool are released. */
typedef struct la_ranked_item {
	const playitem *item;
	double score;
	size_t order;
} la_ranked_item;

LASTFMAPI_DECL
void lastfmapi_query_cache_free(la_query_cache **cache)
{
	la_query_cache *cur, *tmp;
	HASH_ITER(hh, *cache, cur, tmp)
	{
		HASH_DEL(*cache, cur);
		xfree(cur->url);
		xfree(cur->data);
		xfree(cur);
	}
}

struct lastfmapi_source {
	source src;
	u8s username;

	CURL *curl;

	char *base_url;
	char *key;
	char *period;

	lastfmapi_diffusion_strategy strategy;
	bool request_failed;
	/* Per diffusion attempt; the initial history request is outside this budget. */
	bool diffusion_active, budget_exhausted;
	uint64_t request_budget, requests_used;
	la_query_cache *queries;
	la_ranked_item *ranking;
};

LASTFMAPI_DECL
void lastfmapi_ranking_clear(lastfmapi_source *ls)
{
	if (ls->ranking)
		da_free(ls->ranking);
	ls->ranking = NULL;
}

LASTFMAPI_DECL
void lastfmapi_capture_ranking(lastfmapi_source *ls, la_track *const *candidates, size_t count, playlist batch)
{
	/* Raw callbacks without a collection finalizer need no retained ranking. */
	if (!ls->src.finalize_result || !da_len(batch))
		return;
	if (!ls->ranking)
		da_init(ls->ranking, sizeof(*ls->ranking));
	track_key_set seen = {0};
	size_t next = 0;
	/* Mirror the selector's stable deduplication. Batch addresses are now final;
	 * owning source-result segments keep these references valid until merging. */
	for (size_t i = 0; i < count && next < da_len(batch); i++) {
		const track_key *key = candidates[i]->key.identity;
		if (!key || !track_key_set_add(&seen, key))
			continue;
		la_ranked_item item = {
			.item = &batch[next++], .score = candidates[i]->score, .order = da_len(ls->ranking)};
		da_append(ls->ranking, item);
	}
	assert(next == da_len(batch));
	track_key_set_free(&seen);
}

LASTFMAPI_DECL
int lastfmapi_ranked_item_cmp(const void *a, const void *b)
{
	const la_ranked_item *left = a, *right = b;
	if (left->score > right->score)
		return -1;
	if (left->score < right->score)
		return 1;
	return (left->order > right->order) - (left->order < right->order);
}

LASTFMAPI_DECL
void lastfmapi_finalize_result(source *s, struct source_result *result)
{
	lastfmapi_source *ls = (lastfmapi_source *)s;
	if (result && da_len(ls->ranking)) {
		qsort(ls->ranking, da_len(ls->ranking), sizeof(*ls->ranking), lastfmapi_ranked_item_cmp);
		track_key_set seen = {0};
		playlist_viewer *view = &result->viewer;
		view->len = 0;
		for (size_t i = 0; i < da_len(ls->ranking); i++) {
			const playitem *item = ls->ranking[i].item;
			track_key key = {0};
			if (track_key_build(item->tr, &key) < 0)
				continue;
			bool added = track_key_set_add(&seen, &key);
			track_key_free(&key);
			if (added) {
				assert(view->len < view->cap);
				view->unique[view->len++] = item;
			}
		}
		track_key_set_free(&seen);
	}
	lastfmapi_ranking_clear(ls);
}

LASTFMAPI_DECL
char *kv_dump(kv_t *kv)
{
	if (kv->key == NULL)
		return NULL;
	if (kv->val == NULL)
		return xstrdup(kv->key);
	size_t len = strlen(kv->key) + strlen(kv->val) + 1;
	char *res = xmalloc(len + 1);
	xsnprintf(res, len + 1, "%s=%s", kv->key, kv->val);
	res[len] = '\0';
	return res;
}

// curl function
LASTFMAPI_DECL
char *lastfmapi_parse_url(const char *base_url, const char *method, unsigned para_count, va_list ap)
{
	va_list cp;
	unsigned i;
	char *url, *method_str = NULL, *hout = NULL;
	CURLU *h = NULL;

	h = curl_url();
	kv_t method_pair = MAKE_KV("method", method);
	method_str = kv_dump(&method_pair);
	if (method_str == NULL)
		mxrec_cleanup(cleanup, url, 0);
	curl_url_set(h, CURLUPART_URL, base_url, 0);
	curl_url_set(h, CURLUPART_QUERY, method_str, CURLU_APPENDQUERY);

	va_copy(cp, ap);
	for (i = 0; i < para_count; ++i) {
		kv_t pair = va_arg(cp, kv_t);
		char *kv_str = kv_dump(&pair);
		if (kv_str == NULL)
			mxrec_cleanup(cleanup, url, 0);
		curl_url_set(h, CURLUPART_QUERY, kv_str, CURLU_APPENDQUERY | CURLU_URLENCODE);
		xfree(kv_str);
	}

	curl_url_get(h, CURLUPART_URL, &hout, 0);
	url = xstrdup(hout);

cleanup:
	xfree(method_str);
	va_end(cp);
	if (hout)
		curl_free(hout);
	if (h)
		curl_url_cleanup(h);
	return url;
}

LASTFMAPI_DECL
int lastfmapi_xfer_cb(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	source *s = userdata;
	size_t done, total;
	(void)ultotal;
	(void)ulnow;
	done = (size_t)dlnow;
	total = (size_t)dltotal;
	if (total == 0)
		return 0;
	s->ur(s->update_entry, done, total, "get recent tracks");
	return 0;
}

LASTFMAPI_DECL
int lastfmapi_curl(curlbuf *buf, lastfmapi_source *ls, bool update, const char *method, unsigned para_count, ...)
{
	int ret;
	CURLcode code;
	long http_code = 0;
	va_list paras;
	char *url;
	CURL *curl = ls->curl;
	const char *base_url = ls->base_url;
	source *s = &ls->src;

#ifdef LASTFMAPI_DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
#endif

	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)s->timeout);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfmapi_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

	va_start(paras, para_count);
	url = lastfmapi_parse_url(base_url, method, para_count, paras);
	va_end(paras);
	if (url == NULL)
		mxrec_cleanup(cleanup, ret, -1);
	if (ls->diffusion_active) {
		la_query_cache *cached;
		HASH_FIND_STR(ls->queries, url, cached);
		if (cached) {
			curlbuf_append(buf, cached->data, cached->len);
			mxrec_cleanup(cleanup, ret, 0);
		}
		if (ls->requests_used >= ls->request_budget) {
			ls->budget_exhausted = true;
			mxrec_cleanup(cleanup, ret, 1); /* Intentional stop, not request failure. */
		}
		++ls->requests_used;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);

	if (update && s->ur) {
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, lastfmapi_xfer_cb);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, s);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	}

	code = curl_easy_perform(curl);
	if (update && s->uc) {
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, NULL);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		s->uc(s->update_entry);
	}
	if (code != CURLE_OK) {
		mxrec_error_add(s->attempt_error, method, "curl error %d: %s", (int)code, curl_easy_strerror(code));
		mxrec_cleanup(cleanup, ret, -1);
	}

	curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &http_code);
	if (http_code != 200) {
		mxrec_error_add(s->attempt_error, method, "HTTP status %ld", http_code);
		mxrec_cleanup(cleanup, ret, -2);
	}
	yyjson_doc *response = yyjson_read(buf->buf, buf->len, 0);
	if (response) {
		yyjson_val *root = yyjson_doc_get_root(response);
		yyjson_val *api_error = yyjson_obj_get(root, "error");
		if (api_error) {
			mxrec_error_add(s->attempt_error, method, "Last.fm API error code %d",
					yyjson_get_int(api_error));
			yyjson_doc_free(response);
			mxrec_cleanup(cleanup, ret, -1);
		}
		if (ls->diffusion_active) {
			/* Cache response bytes, not nodes: each path keeps its own parent,
			 * depth and score. Failed requests and invalid JSON are not cached. */
			la_query_cache *cached = xcalloc(1, sizeof(*cached));
			cached->url = xstrdup(url);
			cached->data = xmalloc(buf->len);
			cached->len = buf->len;
			memcpy(cached->data, buf->buf, buf->len);
			HASH_ADD_KEYPTR(hh, ls->queries, cached->url, strlen(cached->url), cached);
		}
		yyjson_doc_free(response);
	}
	ret = 0;
cleanup:
	if (ret < 0)
		ls->request_failed = true;
	xfree(url);
	return ret;
}

LASTFMAPI_DECL
int lastfmapi_source_init(void *sp, config_t *cfg)
{
	globalCurlInit();
	lastfmapi_source *s = (lastfmapi_source *)sp;
	memset(s, 0, sizeof(*s));
	s->src = __lastfmapi_source;
	source_init(&s->src, cfg);
	/* This source deliberately ignores general:timeout. */
	s->src.timeout = 0;
	s->src.merge_weight = cfg->lastfmapi_merge_weight;

	if (lastfm_security_init(cfg) < 0)
		return -1;

	s->curl = curl_easy_init();
	if (s->curl == NULL)
		return -1;
	source_perform_security(s);

	s->username = u8sdup(cfg->lastfm_username);
	s->base_url = xstrdup(cfg->lastfmapi_base_url);
	s->key = xstrdup(cfg->lastfmapi_key);
	s->period = xstrdup(cfg->lastfmapi_period);
	s->strategy = str2strategy(cfg->lastfmapi_strategy, cfg->lastfmapi_sample);
	return source_check(s) ? 0 : -1;
}

extern int lastfmapi_source_new(source **src, config_t *cfg)
{
	assert(cfg);
	*src = NULL;
	void *s = xmalloc(sizeof(struct lastfmapi_source));
	if (lastfmapi_source_init(s, cfg) < 0) {
		lastfmapi_source_destroy(s);
		xfree(s);
		return -1;
	}
	*src = s;
	return 0;
}

LASTFMAPI_DECL
int lastfmapi_diffusion_recomm(lastfmapi_source *ls, recomm_option opts, la_track *map, playlist *p, time_t period,
			       la_identity_pool *identities)
{
	int ret;
	size_t i;
	size_t idx, *idxs = NULL, map_size;
	size_t src_size;
	lastfmapi_diffusion_strategy strategy = ls->strategy;
	if (!lastfmapi_strategy_valid(strategy)) {
		mxrec_error_add(opts.error, "Last.fm API configuration", "invalid diffusion strategy or seed sampling");
		return -1;
	}
	if (!map) {
		playlist empty = NULL;
		da_init(empty, sizeof(*empty));
		*p = empty;
		return 0;
	}
#ifdef LASTFMAPI_DEBUG
	fprintf(stderr, "origin hash map size is %u\n", HASH_CNT(hh, map));
#endif
	la_track *cur, **src;
	map_size = HASH_CNT(hh, map);
	size_t seed_count = opts.lastfmapi_opts.seed_count < map_size ? opts.lastfmapi_opts.seed_count : map_size;
	src_size = 0;
	src = xmalloc((seed_count + 1) * sizeof(*src));
	// sentinel
	memset(src, 0, (seed_count + 1) * sizeof(*src));
	// Top N diffusion or RANDOM
	if (strategy & LASTFMAPI_DIFFUSION_TOPN) {
		for (i = 0, cur = map; cur && i < seed_count; ++i, cur = cur->hh.next) {
			src[i] = cur;
		}
		src_size = i;
	} else if (strategy & LASTFMAPI_DIFFUSION_RAMSAMPLE) {
		map_size = HASH_CNT(hh, map);
		idxs = xmalloc(sizeof(*idxs) * seed_count);
		assert(seed_count > 0);
		int sampled = _lastfmapi_recomm_random_sample(seed_count, idxs, 0, map_size - 1,
							      opts.lastfmapi_opts.random_lambda);
		if (sampled <= 0) {
			mxrec_error_add(opts.error, "Last.fm API seeds", "failed to sample random seeds");
			mxrec_cleanup(cleanup, ret, -1);
		}
		seed_count = (size_t)sampled;

		for (idx = 0, i = 0, cur = map; cur && idx < seed_count; ++i, cur = cur->hh.next) {
			if (i == idxs[idx]) {
				src[idx] = cur;
				++idx;
			}
		}
		src_size = idx;
	}

	if (!src_size) {
		playlist empty = NULL;
		da_init(empty, sizeof(*empty));
		*p = empty;
		ret = 0;
		goto cleanup;
	}
	ls->diffusion_active = true;
	ls->budget_exhausted = false;
	ls->requests_used = 0;
	ls->request_budget = opts.lastfmapi_opts.request_budget;
	ret = _lastfmapi_diffusion_core(src, src_size, strategy, opts.lastfmapi_opts.diffusion, period, map, p, ls,
					opts, identities);
	ls->diffusion_active = false;
	lastfmapi_query_cache_free(&ls->queries);
	if (ls->budget_exhausted)
		mxrec_error_add(opts.error, "Last.fm API diffusion",
				"request budget exhausted (%llu/%llu); candidates retained",
				(unsigned long long)ls->requests_used, (unsigned long long)ls->request_budget);
cleanup:
	xfree(idxs);
	xfree(src);
	return ret;
}

LASTFMAPI_DECL
int _lastfmapi_diffusion_core(la_track **src, size_t src_size, lastfmapi_diffusion_strategy strategy, int diffusion,
			      time_t period, la_track *map, playlist *p_ref, lastfmapi_source *ls, recomm_option opts,
			      la_identity_pool *identities)
{
	source *s = &ls->src;
	assert(src_size > 0);
	diffusion_seed *seeds = xcalloc(src_size, sizeof(*seeds));
	diffusion_nodes nodes = {0};
	la_track **res = NULL;
	int ret = -1;
	size_t active = src_size, started = 0, finished = 0, processed = 0, pending = src_size;
	da_init(nodes.owned, sizeof(*nodes.owned));
	da_init(res, sizeof(*res));
	for (size_t i = 0; i < src_size; i++) {
		seeds[i].work = strategy & LASTFMAPI_DIFFUSION_DFS ? df_stack : df_queue;
		df_deque_init(&seeds[i].work, 8);
		seeds[i].work.push(&seeds[i].work, src[i]);
	}

	/* Seed scheduling is round robin. DFS/BFS only controls which node that
	 * seed pops on its turn; its remaining frontier survives every yield. */
	while (active && !ls->budget_exhausted && !nodes.exhausted) {
		for (size_t i = 0; i < src_size && !ls->budget_exhausted && !nodes.exhausted; i++) {
			diffusion_seed *seed = &seeds[i];
			if (df_deque_empty(&seed->work))
				continue;
			if (!seed->started) {
				seed->started = true;
				started++;
			}
			size_t before = seed->work.size;
			_lastfmapi_diffusion_step(&seed->work, diffusion, map, &res, ls, opts, &nodes, identities);
			pending = pending - before + seed->work.size;
			if (!ls->budget_exhausted && !nodes.exhausted)
				processed++;
			if (df_deque_empty(&seed->work)) {
				finished++;
				active--;
			}
			if (opts.progress_bar && s->ur) {
				char desc[128];
				snprintf(desc, sizeof(desc), "seeds %zu/%zu; done %zu%s", started, src_size, finished,
					 ls->budget_exhausted || nodes.exhausted ? " (budget)" : "");
				/* Always count processed/pending path nodes, including at stop. */
				s->ur(s->update_entry, processed, processed + pending, desc);
			}
		}
	}
	if (opts.progress_bar && s->uc)
		s->uc(s->update_entry);

	if (_lastfmapi_latracks_score(res, da_len(res), opts.lastfmapi_opts.diff_lambda, period,
				      opts.lastfmapi_opts.score_beta) < 0)
		goto cleanup;
	qsort(res, da_len(res), sizeof(*res), la_track_score_cmp);
#ifdef LASTFMAPI_DEBUG
	fprintf(stderr, "res of diff core\n");
	for (size_t i = 0; i < da_len(res); i++)
		la_track_dump(stderr, res[i]);
#endif
	ret = lastfmapi_select_unique(res, da_len(res), da_len(res), p_ref, opts.strict);
	if (ret >= 0)
		lastfmapi_capture_ranking(ls, res, da_len(res), *p_ref);
cleanup:
	if (nodes.exhausted)
		mxrec_error_add(opts.error, "Last.fm API diffusion",
				"node budget exhausted (%zu/%llu); candidates retained", da_len(nodes.owned),
				(unsigned long long)opts.lastfmapi_opts.node_budget);
	for (size_t i = 0; i < src_size; i++)
		df_deque_free(&seeds[i].work);
	xfree(seeds);
	for (size_t i = 0; i < da_len(nodes.owned); i++)
		la_track_free(nodes.owned[i]);
	da_free(nodes.owned);
	da_free(res);
	return ret;
}

/* Consume a parsed child array. The attempt owns every accepted node, while
 * only songs outside history enter the candidate list. Frontier entries borrow
 * these nodes, so pending paths stay valid until final scoring and cleanup. */
LASTFMAPI_DECL
void lastfmapi_diffusion_keep(la_track **children, size_t count, df_deque *work, const la_track *map, la_track ***res,
			      diffusion_nodes *nodes, uint64_t node_budget)
{
	for (size_t i = 0; i < count; i++) {
		la_track *child = children[i], *history;
		if (da_len(nodes->owned) >= node_budget) {
			nodes->exhausted = true;
			la_track_free(child);
			continue;
		}
		da_append(nodes->owned, child);
		work->push(work, child);
		HASH_FIND_BYHASHVALUE(hh, map, child->key.identity->data, child->key.identity->len,
				      child->key.identity->hash, history);
		if (!history)
			da_append(*res, child);
		else
			child->ruts = history->ruts;
	}
	xfree(children);
}

/* Expand at most one node. An unfinished node is put back when a shared budget
 * stops the attempt, so progress still counts that work as pending. */
LASTFMAPI_DECL
void _lastfmapi_diffusion_step(df_deque *work, int diffusion, const la_track *map, la_track ***res,
			       lastfmapi_source *ls, recomm_option opts, diffusion_nodes *nodes,
			       la_identity_pool *identities)
{
	la_track *cur = work->pop(work);
	if (cur->diffusion >= diffusion)
		return;
	la_track **children = NULL;
	size_t count =
		_lastfmapi_get_track_similar(cur, opts.lastfmapi_opts.diff_size, &children, ls, opts, identities);
	lastfmapi_diffusion_keep(children, count, work, map, res, nodes, opts.lastfmapi_opts.node_budget);
	if (!ls->budget_exhausted && !nodes->exhausted) {
		children = NULL;
		count = _lastfmapi_diffusion_artist(cur, diffusion, opts.lastfmapi_opts.diff_size, &children, ls, opts,
						    identities);
		lastfmapi_diffusion_keep(children, count, work, map, res, nodes, opts.lastfmapi_opts.node_budget);
	}
	if (ls->budget_exhausted || nodes->exhausted)
		work->push(work, cur);
}

LASTFMAPI_DECL
la_track *lastfmapi_json2similar_latrack(yyjson_val *val, bool strict, mxrec_error *jerr, la_identity_pool *identities)
{
	la_track *ltr;
	yyjson_val *jar;
	const char *name, *ar, *tr_mbid, *ar_mbid;
	double match;

	name = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (name == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	if (!lastfmapi_match(yyjson_obj_get(val, "match"), &match)) {
		mxrec_error_add(jerr, "track.getSimilar", "invalid match; expected a finite number in [0,1]");
		return NULL;
	}

	jar = yyjson_obj_get(val, "artist");
	ar = yyjson_get_str(yyjson_obj_get(jar, "name"));
	if (ar == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	tr_mbid = yyjson_get_str(yyjson_obj_get(val, "mbid"));
	ar_mbid = yyjson_get_str(yyjson_obj_get(jar, "mbid"));

	ltr = la_track_new(name, ar, tr_mbid, ar_mbid, identities);
	ltr->ruts = 0;
	ltr->match = match;
	if (!ltr->key.identity) {
		la_track_free(ltr);
		ltr = NULL;
	}
cleanup:
	return ltr;
}

LASTFMAPI_DECL
size_t _lastfmapi_get_track_similar(la_track *ltr, unsigned diff_size, la_track ***res, lastfmapi_source *ls,
				    recomm_option opts, la_identity_pool *identities)
{
	size_t i, track_size;
	la_track **_res = NULL;
	yyjson_read_err err;
	yyjson_doc *doc = NULL;
	yyjson_val *root, *tracks, *track;
	mxrec_error *jerr = opts.error;
	curlbuf buf;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	char diff_size_s[LONG_STR_SIZE];
	ull2string(diff_size_s, LONG_STR_SIZE, diff_size);

	if (lastfmapi_curl(&buf, ls, false, LASTFMAPI_TRACK_GETSIMILAR, 6, MAKE_KV("artist", (char *)ltr->key.artist),
			   MAKE_KV("track", (char *)ltr->key.name), MAKE_KV("mbid", (char *)ltr->key.tr_mbid),
			   MAKE_KV("api_key", ls->key), MAKE_KV("limit", diff_size_s),
			   MAKE_KV("format", LASTFMAPI_FORMAT)) != 0) {
		mxrec_cleanup(cleanup, track_size, 0);
	}
	doc = yyjson_read_opts(buf.buf, buf.len, 0, NULL, &err);
	if (doc == NULL) {
		write_jsonerr(jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n", JSON_ERR_HEAD,
			      err.msg, err.code, err.pos);
		ls->request_failed = true;
		_res = NULL;
		mxrec_cleanup(cleanup, track_size, 0);
	}

	root = yyjson_doc_get_root(doc);

	tracks = yyjson_obj_get(yyjson_obj_get(root, "similartracks"), "track");
	// track.getsimilar will return 200 OK HTTP status code on error
	if (!yyjson_is_arr(tracks)) {
		mxrec_error_add(jerr, "Last.fm API response", "required track array missing or invalid");
		ls->request_failed = true;
		_res = NULL;
		mxrec_cleanup(cleanup, track_size, 0);
	}
	track_size = yyjson_arr_size(tracks);
	if (track_size == 0) {
		_res = NULL;
		mxrec_cleanup(cleanup, track_size, 0);
	}

	_res = xmalloc(track_size * sizeof(*_res));
	yyjson_arr_foreach(tracks, i, track_size, track)
	{
		_res[i] = lastfmapi_json2similar_latrack(track, opts.strict, jerr, identities);
		if (!_res[i]) {
			mxrec_error_add(jerr, "track.getSimilar", "track entry %zu has invalid title, artist or match",
					i);
			ls->request_failed = true;
			for (size_t k = 0; k < i; k++)
				la_track_free(_res[k]);
			xfree(_res);
			_res = NULL;
			track_size = 0;
			goto cleanup;
		}
		_res[i]->diffusion = ltr->diffusion + 1;
		_res[i]->parent = ltr;
	}

cleanup:
	*res = _res;
	yyjson_doc_free(doc);
	curlbuf_free(&buf);
	return track_size;
}

LASTFMAPI_DECL
la_artist _lastfmapi_json2sim_artist(yyjson_val *val, bool strict, mxrec_error *jerr)
{
	la_artist lar = {0};
	const char *name, *mbid;
	double match;

	name = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (name == NULL)
		write_jsonerr(jerr, "failed to get field \"name\" of artist");

	mbid = yyjson_get_str(yyjson_obj_get(val, "mbid"));

	if (!lastfmapi_match(yyjson_obj_get(val, "match"), &match)) {
		mxrec_error_add(jerr, "artist.getSimilar", "invalid match; expected a finite number in [0,1]");
		return lar;
	}

	la_artist_init(&lar, name, mbid, match);
	return lar;
}

LASTFMAPI_DECL
size_t _lastfmapi_jsonbuf2sim_artists(curlbuf *buf, unsigned diffusion, bool strict, la_artist **res, mxrec_error *jerr)
{
	size_t i, ar_size;
	la_artist *_res = NULL;
	yyjson_doc *doc;
	yyjson_read_err err;
	yyjson_val *root, *ars, *ar;

	doc = yyjson_read_opts(buf->buf, buf->len, 0, NULL, &err);
	if (doc == NULL) {
		write_jsonerr(jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n", JSON_ERR_HEAD,
			      err.msg, err.code, err.pos);
		_res = NULL;
		mxrec_cleanup(cleanup, ar_size, 0);
	}

	root = yyjson_doc_get_root(doc);
	ars = yyjson_obj_get(yyjson_obj_get(root, "similarartists"), "artist");
	// artist.getsimilar will return 200 OK HTTP status code on error
	if (!yyjson_is_arr(ars)) {
		mxrec_error_add(jerr, "artist.getSimilar", "required artist array missing or invalid");
		_res = NULL;
		mxrec_cleanup(cleanup, ar_size, 0);
	}

	ar_size = yyjson_arr_size(ars);
	if (ar_size == 0) {
		_res = NULL;
		mxrec_cleanup(cleanup, ar_size, 0);
	}

	_res = xmalloc(ar_size * sizeof(*_res));
	yyjson_arr_foreach(ars, i, ar_size, ar)
	{
		_res[i] = _lastfmapi_json2sim_artist(ar, strict, jerr);
		if (!_res[i].name) {
			for (size_t k = 0; k <= i; k++)
				la_artist_cleanup(&_res[k]);
			xfree(_res);
			_res = NULL;
			ar_size = 0;
			goto cleanup;
		}
		_res[i].diffusion = diffusion + 1;
	}

cleanup:
	*res = _res;
	yyjson_doc_free(doc);
	return ar_size;
}

LASTFMAPI_DECL
la_track *_lastfmapi_json2top_track(yyjson_val *val, bool strict, mxrec_error *jerr, la_identity_pool *identities)
{
	yyjson_val *ar;
	la_track *ltr;
	const char *name, *artist, *tr_mbid, *ar_mbid;
	name = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (name == NULL) {
		write_jsonerr(jerr, "failed to get field \"name\" of top track");
		mxrec_cleanup(cleanup, ltr, 0);
	}
	tr_mbid = yyjson_get_str(yyjson_obj_get(val, "mbid"));

	ar = yyjson_obj_get(val, "artist");
	artist = yyjson_get_str(yyjson_obj_get(ar, "name"));
	if (artist == NULL) {
		write_jsonerr(jerr, "failed to get field \"artist.name\" of top track");
		mxrec_cleanup(cleanup, ltr, 0);
	}
	ar_mbid = yyjson_get_str(yyjson_obj_get(ar, "mbid"));

	ltr = la_track_new(name, artist, tr_mbid, ar_mbid, identities);
	if (!ltr->key.identity) {
		la_track_free(ltr);
		ltr = NULL;
	}
cleanup:
	return ltr;
}

LASTFMAPI_DECL
size_t _lastfmapi_artist_top_tracks(const la_artist *ar, unsigned diff_size, la_track *parent, la_track ***res,
				    lastfmapi_source *ls, recomm_option opts, mxrec_error *jerr,
				    la_identity_pool *identities)
{
	size_t i, tr_size;
	la_track **_res = NULL;
	char diff_size_s[LONG_STR_SIZE];
	ull2string(diff_size_s, LONG_STR_SIZE, diff_size);
	curlbuf buf;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);

	yyjson_doc *doc = NULL;
	yyjson_read_err err;
	yyjson_val *root, *tracks, *tr;

	if (lastfmapi_curl(&buf, ls, false, LASTFMAPI_ARTIST_GETTOPTRACKS, 5, MAKE_KV("artist", (char *)ar->name),
			   MAKE_KV("mbid", ar->mbid), MAKE_KV("api_key", ls->key), MAKE_KV("limit", diff_size_s),
			   MAKE_KV("format", LASTFMAPI_FORMAT)) != 0) {
		_res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	doc = yyjson_read_opts(buf.buf, buf.len, 0, 0, &err);
	if (doc == NULL) {
		write_jsonerr(jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n", JSON_ERR_HEAD,
			      err.msg, err.code, err.pos);
		ls->request_failed = true;
		_res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	root = yyjson_doc_get_root(doc);
	tracks = yyjson_obj_get(yyjson_obj_get(root, "toptracks"), "track");

	// artist.gettoptracks will return 200 OK HTTP status code on error
	if (!yyjson_is_arr(tracks)) {
		mxrec_error_add(jerr, "Last.fm API response", "required track array missing or invalid");
		ls->request_failed = true;
		_res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}
	tr_size = yyjson_arr_size(tracks);
	if (tr_size == 0) {
		_res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	_res = xmalloc(tr_size * sizeof(*_res));
	yyjson_arr_foreach(tracks, i, tr_size, tr)
	{
		_res[i] = _lastfmapi_json2top_track(tr, opts.strict, jerr, identities);
		if (!_res[i]) {
			mxrec_error_add(jerr, "artist.getTopTracks", "track entry %zu has invalid title or artist", i);
			ls->request_failed = true;
			for (size_t k = 0; k < i; k++)
				la_track_free(_res[k]);
			xfree(_res);
			_res = NULL;
			tr_size = 0;
			goto cleanup;
		}
		_res[i]->diffusion = ar->diffusion + 1;
		_res[i]->match = ar->match;
		_res[i]->parent = parent;
	}

cleanup:
	*res = _res;
	yyjson_doc_free(doc);
	curlbuf_free(&buf);
	return tr_size;
}

LASTFMAPI_DECL
size_t _lastfmapi_all_artists_top_tracks(const la_artist *ars, size_t ar_size, unsigned diff_size, la_track *parent,
					 la_track ***res, lastfmapi_source *ls, recomm_option opts, mxrec_error *jerr,
					 la_identity_pool *identities)
{
	size_t i, total_size, tr_size;
	la_track **trs, **total_res = NULL, **_res;
	da_init(total_res, sizeof(*total_res));

	for (i = 0; i < ar_size && !ls->budget_exhausted; ++i) {
		tr_size = _lastfmapi_artist_top_tracks(&ars[i], diff_size, parent, &trs, ls, opts, jerr, identities);
		if (tr_size)
			da_append_arr(total_res, trs, tr_size);
		xfree(trs);
	}

	total_size = da_len(total_res);
	if (total_size == 0) {
		_res = NULL;
		mxrec_cleanup(cleanup, total_size, 0);
	}

	_res = xmalloc(total_size * sizeof(*_res));
	memcpy(_res, total_res, total_size * sizeof(*total_res));
cleanup:
	da_free(total_res);
	*res = _res;
	return total_size;
}

LASTFMAPI_DECL
u8s _lastfmapi_artist_first(const la_track *ltr)
{
	u8s_ssize_t len;
	u8s str = ltr->key.artist;
	u8s find = u8schr(str, u8cpdecode("/"));
	if (find) {
		assert(!ltr->key.ar_mbid || !strlen(ltr->key.ar_mbid));
		len = find - str;
		return u8snewlen(str, len);
	}
	return u8sdup(str);
}

LASTFMAPI_DECL
size_t _lastfmapi_diffusion_artist(la_track *ltr, int diffusion, unsigned diff_size, la_track ***res,
				   lastfmapi_source *ls, recomm_option opts, la_identity_pool *identities)
{
	size_t i, ar_size = 0, tr_size;
	la_artist *ars = NULL;
	u8s artist = NULL;
	curlbuf buf;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	char diff_size_s[LONG_STR_SIZE];
	diff_size = (unsigned)sqrt(diff_size);
	ull2string(diff_size_s, LONG_STR_SIZE, diff_size);
	mxrec_error *jerr = opts.error;

	if (ltr->diffusion >= diffusion - 1) {
		*res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	// here simply get the first artist
	artist = _lastfmapi_artist_first(ltr);
	if (lastfmapi_curl(&buf, ls, false, LASTFMAPI_ARTIST_GETSIMILAR, 5, MAKE_KV("artist", (char *)artist),
			   MAKE_KV("mbid", (char *)ltr->key.ar_mbid), MAKE_KV("api_key", ls->key),
			   MAKE_KV("limit", diff_size_s), MAKE_KV("format", LASTFMAPI_FORMAT)) != 0) {
		*res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	size_t error_count = jerr ? jerr->count + jerr->omitted : 0;
	ar_size = _lastfmapi_jsonbuf2sim_artists(&buf, ltr->diffusion, opts.strict, &ars, jerr);
	if (jerr && jerr->count + jerr->omitted > error_count)
		ls->request_failed = true;
	if (!ar_size && !ars) {
		*res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	tr_size = _lastfmapi_all_artists_top_tracks(ars, ar_size, diff_size, ltr, res, ls, opts, jerr, identities);

cleanup:
	u8sfree(artist);
	for (i = 0; i < ar_size; ++i) {
		la_artist_cleanup(&ars[i]);
	}
	xfree(ars);
	curlbuf_free(&buf);
	return tr_size;
}

#define DEFAULT_SCORE 100.0
LASTFMAPI_DECL
int _lastfmapi_latrack_score(la_track *ltr, double lambda, time_t period, double beta)
{
	if (!ltr)
		return 0;
	const la_track *cur = ltr;
	double similarity = 1.0;
	unsigned hops = 0;
	/* Roots have no incoming similarity edge. All other nodes contribute
	 * exactly one path edge; never include the root's default match=0. */
	while (cur->parent) {
		similarity *= cur->match;
		++hops;
		cur = cur->parent;
	}
	double age = period > 0 ? fmin(1.0, fmax(0.0, (double)cur->ruts / period)) : 0;
	/* Artist -> top-track adds two diffusion levels but only one match.
	 * Penalize actual diffusion depth, including that extra level. */
	unsigned depth = ltr->diffusion > 0 ? (unsigned)ltr->diffusion : hops;
	double depth_weight = exp(-lambda * (depth > 0 ? depth - 1 : 0));
	double recency_weight = 1.0 / (1.0 + beta * age);
	ltr->score = DEFAULT_SCORE * similarity * depth_weight * recency_weight;
	return 0;
}

LASTFMAPI_DECL
int _lastfmapi_latracks_score(la_track **tracks, size_t len, double lambda, time_t period, double beta)
{
	int ret;
	size_t i;
	for (i = 0; i < len; ++i)
		if ((ret = _lastfmapi_latrack_score(tracks[i], lambda, period, beta)) < 0)
			return ret;
	return 0;
}

LASTFMAPI_DECL
int lastfmapi_validate_options(const source *s, const recomm_option *opts, mxrec_error *error)
{
	(void)s;
	int ret = 0;
	if (opts->level != RECOMM_SIMPLE) {
		mxrec_error_add(error, "Last.fm API configuration", "only simple recommendation level is supported");
		ret = -1;
	}
	if (!isfinite(opts->lastfmapi_opts.random_lambda) || opts->lastfmapi_opts.random_lambda <= 0) {
		mxrec_error_add(error, "Last.fm API configuration", "random-sample-lambda must be finite and positive");
		ret = -1;
	}
	if (!isfinite(opts->lastfmapi_opts.diff_lambda) || opts->lastfmapi_opts.diff_lambda < 0) {
		mxrec_error_add(error, "Last.fm API configuration", "diffusion-lambda must be finite and nonnegative");
		ret = -1;
	}
	if (!isfinite(opts->lastfmapi_opts.score_beta) || opts->lastfmapi_opts.score_beta < 0) {
		mxrec_error_add(error, "Last.fm API configuration", "score-beta must be finite and nonnegative");
		ret = -1;
	}
	if (!opts->lastfmapi_opts.seed_count || opts->lastfmapi_opts.seed_count > INT_MAX ||
	    !opts->lastfmapi_opts.request_budget || !opts->lastfmapi_opts.node_budget ||
	    opts->lastfmapi_opts.node_budget > INT_MAX || opts->lastfmapi_opts.diffusion > INT_MAX ||
	    !opts->lastfmapi_opts.diff_size || opts->lastfmapi_opts.diff_size > UINT_MAX) {
		mxrec_error_add(error, "Last.fm API configuration",
				"requires seed-count in [1,INT_MAX], request-budget > 0, node-budget in [1,INT_MAX], "
				"diffusion <= INT_MAX and diff-size in [1,UINT_MAX]");
		ret = -1;
	}
	return ret;
}

// interface implement
LASTFMAPI_DECL
void lastfmapi_source_destroy(void *sp)
{
	lastfmapi_source *s = (lastfmapi_source *)sp;
	lastfm_security_free();
	lastfmapi_query_cache_free(&s->queries);
	lastfmapi_ranking_clear(s);
	u8sfree(s->username);
	xfree(s->base_url);
	xfree(s->key);
	xfree(s->period);
	curl_easy_cleanup(s->curl);
}

LASTFMAPI_DECL
int lastfmapi_recomm_single(source *s, playitem *p, recomm_option opts)
{
	mxrec_error_add(opts.error, "Last.fm API recommendation", "single recommendation is not supported");
	return -1;
}

LASTFMAPI_DECL
int lastfmapi_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	/* API seeds and exploration budgets are independent of requested output size. */
	(void)num;
	mxrec_log("Calling lastfm-api recomm_multi\n");
	int ret;
	curlbuf buf;
	la_track *la_tr_map = NULL;
	la_identity_pool identities = {0};
	time_t period;
	mxrec_error *jerr = opts.error;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	assert(p);

	lastfmapi_source *ls = (lastfmapi_source *)s;
	ls->request_failed = false;

	time_t now = mxrec_now();
	if (now == (time_t)-1)
		mxrec_cleanup(cleanup, ret, -1);
	time_t from = string2timestamp(ls->period);
	if (from < 0 || from >= now) {
		mxrec_error_add(opts.error, "Last.fm API configuration", "period must select a time before now");
		mxrec_cleanup(cleanup, ret, -1);
	}
	// The API uses the start timestamp; scoring uses elapsed seconds.
	period = now - from;
	char from_s[LONG_STR_SIZE];
	size_t str_len = ull2string(from_s, LONG_STR_SIZE, from);

	if (str_len == 0)
		mxrec_cleanup(cleanup, ret, -1);

	from_s[str_len] = '\0';

	if ((ret = lastfmapi_curl(&buf, ls, opts.progress_bar, LASTFMAPI_USER_GETRECENTTRACKS, 4,
				  MAKE_KV("username", (char *)ls->username), MAKE_KV("api_key", ls->key),
				  MAKE_KV("from", from_s), MAKE_KV("format", LASTFMAPI_FORMAT))) < 0)
		mxrec_cleanup(cleanup, ret, ret);

	// recent tracks map
	if ((ret = lastfmapi_jsonbuf2recent_latrack_map(&buf, opts.strict, &la_tr_map, jerr, &identities)) < 0) {
		mxrec_error_add(opts.error, "Last.fm API recommendation", "failed to parse recent tracks");
		mxrec_cleanup(cleanup, ret, ret);
	}
	curlbuf_clear(&buf);

	ret = lastfmapi_diffusion_recomm(ls, opts, la_tr_map, p, period, &identities);
	if (ret >= 0 && ls->request_failed) {
		mxrec_error_add(opts.error, "Last.fm API diffusion", "failed requests; partial results retained");
		ret = -1;
	}

cleanup:
	la_track_map_cleanup(la_tr_map);
	la_identity_pool_free(&identities);
	curlbuf_free(&buf);
	return ret;
}

LASTFMAPI_DECL
void lastfmapi_security_handle(source *s)
{
	CURLcode code = CURLE_OK;
	lastfmapi_source *ls = (lastfmapi_source *)s;
	lastfm_security *lsc = (lastfm_security *)(s->security);
	CURL *curl = ls->curl;
	// impersonate
	code = curl_easy_impersonate(curl, lsc->profile, 1L);
	if (code != CURLE_OK) {
		if (s->attempt_error)
			mxrec_error_add(s->attempt_error, "Last.fm API security", "impersonation failed: %s",
					curl_easy_strerror(code));
		else
			error("Last.fm API impersonation failed: %s", curl_easy_strerror(code));
		curl_easy_reset(curl);
	}
}

#define check(expr, val)                                                                                               \
	do {                                                                                                           \
		if (!(expr)) {                                                                                         \
			(val) = false;                                                                                 \
			error("lastfm-api source init failed on %s", (#expr));                                         \
		}                                                                                                      \
	} while (0)

LASTFMAPI_DECL
bool lastfmapi_config_check(source *s)
{
	bool ret = true;
	lastfmapi_source *ls = (lastfmapi_source *)s;
	check(ls->username != NULL, ret);
	check(ls->base_url != NULL, ret);
	check(ls->key != NULL, ret);
	check(ls->period != NULL, ret);
	check(lastfmapi_strategy_valid(ls->strategy), ret);
	return ret;
}
