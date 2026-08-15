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
#include "playlist.h"
#include "random.h"
#include "source.h"
#include "source/comm-source.h"
#include "track.h"
#include "u8string.h"
#include "uthash.h"
#include "utils/string.h"
#include "utils/time.h"
#include "xmalloc.h"
#include "yyjson/src/yyjson.h"
#include <curl/curl.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define LASTFMAPI_DECL static

#define JSON_ERR_HEAD "LASTFMWEB"

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

typedef struct lastfmapi_track la_track;
struct lastfmapi_track {
	struct tr_key_t {
		u8s name;
		u8s artist;
		// mbid can be NULL
		char *ar_mbid;
		char *tr_mbid;
	} key;
	// used for tracing diffusion
	double score;
	time_t ruts;
	la_track *parent;
	int diffusion;
	double match;
	bool internal, in_result;
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
la_track *la_track_new(const char *name, const char *artist, const char *tr_mbid, const char *ar_mbid)
{
	la_track *ltr;
	ltr = xmalloc(sizeof(*ltr));
	memset(ltr, 0, sizeof(*ltr));
	ltr->key.name = u8snew(name);
	ltr->key.artist = u8snew(artist);
	ltr->key.tr_mbid = xstrdup(tr_mbid);
	ltr->key.ar_mbid = xstrdup(ar_mbid);

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

/**
 * la_track hash settings
 */
LASTFMAPI_DECL
uint64_t fnv1a_hash(const void *data, size_t len)
{
	size_t i;
	const unsigned char *p = data;
	uint64_t h = 14695981039346656037ULL;

	for (i = 0; i < len; ++i) {
		h ^= p[i];
		h *= 1099511628211ULL;
	}
	return h;
}

#define hash_combine(h, k) ((h) ^ ((k) + 0x9e3779b97f4a7c15ULL + ((h) << 12) + ((h) >> 4)))

LASTFMAPI_DECL
unsigned la_track_hash(const struct tr_key_t *key)
{
	uint64_t h;
	h = fnv1a_hash(key->name, u8sblen(key->name));
	h = hash_combine(h, fnv1a_hash(key->artist, u8sblen(key->artist)));
	if (key->ar_mbid)
		h = hash_combine(h, fnv1a_hash(key->ar_mbid, strlen(key->ar_mbid)));
	return (unsigned)h;
}

LASTFMAPI_DECL
int la_track_cmp(const struct tr_key_t *a, const struct tr_key_t *b)
{
	int ret;
	if ((ret = u8scmp(a->name, b->name, NULL)) != 0)
		return ret;
	if ((ret = u8scmp(a->artist, b->artist, NULL)) != 0)
		return ret;

	if (a->ar_mbid == NULL && b->ar_mbid)
		return 0;
	if (a->ar_mbid == NULL)
		return -1;
	if (b->ar_mbid == NULL)
		return 1;
	ret = memcmp(a->ar_mbid, b->ar_mbid, strlen(a->ar_mbid));
	return ret;
}

#undef HASH_FUNCTION
#undef HASH_KEYCMP

#define HASH_FUNCTION(s, len, hashv) (hashv) = la_track_hash((const struct tr_key_t *)s)
#define HASH_KEYCMP(a, b, len) (la_track_cmp((const struct tr_key_t *)a, (const struct tr_key_t *)b))

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
lastfmapi_diffusion_strategy str2strategy(const char *s)
{
	const char *err_strategy;
	if (s == NULL)
		mxrec_cleanup(err, err_strategy, "NULL");
	if (strcmp(s, "bfs") == 0)
		return LASTFMAPI_DIFFUSION_BFS;
	else if (strcmp(s, "dfs") == 0)
		return LASTFMAPI_DIFFUSION_DFS;
	else if (strcmp(s, "topn") == 0)
		return LASTFMAPI_DIFFUSION_TOPN;
	else if (strcmp(s, "random") == 0)
		return LASTFMAPI_DIFFUSION_RAMSAMPLE;
	else
		mxrec_cleanup(err, err_strategy, s);
err:
	error("failed to convert %s into diffusion_strategy", err_strategy);
	return LASTFMAPI_DIFFUSION_ERR;
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

LASTFMAPI_DECL
int _lastfmapi_recomm_random_sample(size_t len, size_t *idxs, size_t lb, size_t ub, double lambda)
{
	size_t ret, i, range;
	double step, sum, factor, *widths = NULL, *rands = NULL;
	double clb, cub;
	range = ub - lb;
	if (range + 1 <= len) {
		for (i = 0; i < len; ++i) {
			idxs[i] = i;
		}
		return len;
	}
	rands = xmalloc(sizeof(*rands) * len);
	if (uniform01Array(rands, len) != 0)
		mxrec_cleanup(cleanup, ret, 0);

	widths = xmalloc(sizeof(*widths) * len);
	step = (double)(range) / len;
	factor = -log(lambda) / (range);
	sum = 0.0;
	for (i = 0; i < len; ++i) {
		widths[i] = exp(-factor * (lb + i * step - lb));
		sum += widths[i];
	}
	for (i = 0; i < len; ++i) {
		widths[i] /= sum;
	}
	clb = lb;
	cub = widths[0] * range;
	for (i = 0; i < len; ++i) {
		idxs[i] = (size_t)(clb + (cub - clb) * rands[i]);
		if (i < len - 1) {
			clb = cub;
			cub += widths[i + 1] * range;
		}
	}
	ret = len;
cleanup:
	xfree(rands);
	xfree(widths);
	return ret;
}

LASTFMAPI_DECL
size_t _lastfmapi_get_track_similar(la_track *ltr, unsigned diff_size, la_track ***res, lastfmapi_source *ls,
				    recomm_option opts);
LASTFMAPI_DECL
size_t _lastfmapi_diffusion_artist(la_track *info, int diffusion, unsigned diff, size_t target, const la_track *map,
				   la_track ***res, lastfmapi_source *ls, recomm_option opts);

LASTFMAPI_DECL
size_t _lastfmapi_diffusion_track(la_track *info, int diffusion, size_t target, lastfmapi_diffusion_strategy strategy,
				  const la_track *map, la_track ***res, lastfmapi_source *ls, recomm_option opts,
				  size_t cur_idx, size_t total_tracks);
LASTFMAPI_DECL
int _lastfmapi_latracks_score(la_track **tracks, size_t len, double lambda, time_t period, double beta);

LASTFMAPI_DECL
void _lastfmapi_diffusion_core(la_track **src, size_t src_size, lastfmapi_diffusion_strategy strategy, int diffusion,
			       time_t period, size_t target, la_track *map, playlist *p_ref, lastfmapi_source *ls,
			       recomm_option opts);

LASTFMAPI_DECL
int lastfmapi_diffusion_recomm(lastfmapi_source *ls, recomm_option opts, la_track *map, playlist *p, time_t period,
			       size_t target);

// la_track json parsing
LASTFMAPI_DECL
la_track *lastfmapi_json2recent_latrack(yyjson_val *val, bool strict, time_t now, struct json_err *err)
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

	ltr = la_track_new(name, ar, tr_mbid, ar_mbid);
	assert(now >= uts);
	ltr->ruts = now - uts;
	ltr->internal = ltr->in_result = false;
cleanup:
	return ltr;
}

LASTFMAPI_DECL
int lastfmapi_jsonbuf2recent_latrack_map(curlbuf *buf, bool strict, la_track **map, struct json_err *jerr)
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
		ltr = lastfmapi_json2recent_latrack(track, strict, now, jerr);
		if (ltr == NULL) {
			if (strict) {
				write_jsonerr(jerr, "[%s]: failed to get track", JSON_ERR_HEAD);
				mxrec_cleanup(cleanup, ret, -1);
			}
			continue;
		}
		HASH_FIND(hh, hm, &ltr->key, sizeof(struct tr_key_t), find);
		if (find != NULL) {
			la_track_free(ltr);
			continue;
		}
		HASH_ADD(hh, hm, key, sizeof(struct tr_key_t), ltr);
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
	.destroy = lastfmapi_source_destroy,
	.rsp = lastfmapi_recomm_single,
	.rmp = lastfmapi_recomm_multi,
	.sh = lastfmapi_security_handle,
	.cc = lastfmapi_config_check,
	.security = &__lastfm_security,
};

struct lastfmapi_source {
	source src;
	u8s username;

	CURL *curl;

	char *base_url;
	char *key;
	char *period;

	lastfmapi_diffusion_strategy strategy;
};

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
void lastfmapi_curl_error(FILE *fp, long http_code, curlbuf *buf)
{
	// TODO
	switch (http_code) {
	case 200:
		return;
	}
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

	/**
	 * shorter timeout threshold than the normal one to limit diffusion runtime
	 */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, s->timeout / 5);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfmapi_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

	va_start(paras, para_count);
	url = lastfmapi_parse_url(base_url, method, para_count, paras);
	va_end(paras);
	if (url == NULL)
		mxrec_cleanup(cleanup, ret, -1);
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
		error("lastfm api curl failed: %s", curl_easy_strerror(code));
		mxrec_cleanup(cleanup, ret, -1);
	}

	curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &http_code);
	if (http_code != 200)
		mxrec_cleanup(err, ret, -2);

	ret = 0;
err:
	lastfmapi_curl_error(stderr, http_code, buf);
cleanup:
	xfree(url);
	return ret;
}

LASTFMAPI_DECL
int lastfmapi_source_init(void *sp, config_t *cfg)
{
	globalCurlInit();
	lastfmapi_source *s = (lastfmapi_source *)sp;
	s->src = __lastfmapi_source;

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
	s->strategy = str2strategy(cfg->lastfmapi_strategy) | str2strategy(cfg->lastfmapi_sample);
	return source_check(s) ? 0 : -1;
}

extern int lastfmapi_source_new(source **src, config_t *cfg)
{
	assert(cfg);
	*src = NULL;
	void *s = xmalloc(sizeof(struct lastfmapi_source));
	source_init(s, cfg);
	if (lastfmapi_source_init(s, cfg) < 0) {
		xfree(s);
		return -1;
	}
	*src = s;
	return 0;
}

LASTFMAPI_DECL
int lastfmapi_diffusion_recomm(lastfmapi_source *ls, recomm_option opts, la_track *map, playlist *p, time_t period,
			       size_t target)
{
	int ret;
	size_t i;
	size_t idx, *idxs = NULL, map_size;
	size_t src_size;
	lastfmapi_diffusion_strategy strategy = ls->strategy;
#ifdef LASTFMAPI_DEBUG
	fprintf(stderr, "origin hash map size is %u\n", HASH_CNT(hh, map));
#endif
	la_track *cur, **src;
	src = xmalloc((target + 1) * sizeof(*src));
	// sentinel
	memset(src, 0, (target + 1) * sizeof(*src));
	// Top N diffusion or RANDOM
	if (strategy & LASTFMAPI_DIFFUSION_TOPN) {
		for (i = 0, cur = map; cur && i < target; ++i, cur = cur->hh.next) {
			src[i] = cur;
		}
		src_size = i;
	} else if (strategy & LASTFMAPI_DIFFUSION_RAMSAMPLE) {
		map_size = HASH_CNT(hh, map);
		idxs = xmalloc(sizeof(*idxs) * target);
		assert(target > 0);
		if (_lastfmapi_recomm_random_sample(target, idxs, 0, map_size - 1, opts.lastfmapi_opts.random_lambda) ==
		    0)
			mxrec_cleanup(cleanup, ret, -1);

		for (idx = 0, i = 0, cur = map; cur && idx < target; ++i, cur = cur->hh.next) {
			if (i == idxs[idx]) {
				src[idx] = cur;
				++idx;
			}
		}
		src_size = idx;
	}

	assert(src[0]);
	_lastfmapi_diffusion_core(src, src_size, strategy, opts.lastfmapi_opts.diffusion, period, target, map, p, ls,
				  opts);
	ret = da_len(*p);
cleanup:
	xfree(idxs);
	xfree(src);
	return ret;
}

LASTFMAPI_DECL
void _lastfmapi_diffusion_core(la_track **src, size_t src_size, lastfmapi_diffusion_strategy strategy, int diffusion,
			       time_t period, size_t target, la_track *map, playlist *p_ref, lastfmapi_source *ls,
			       recomm_option opts)
{
	source *s = &ls->src;
	size_t i, child_size, diff_num;
	la_track *cur, **res, **child;
	char progress_desc[256];
	playlist pl = NULL;
	playitem pi;
	res = NULL;
	diff_num = 0;
	da_init(res, sizeof(*res));
	for (i = 0, cur = *src; cur; cur = *(++src), ++i) {
		child_size = _lastfmapi_diffusion_track(cur, diffusion, target, strategy, map, &child, ls, opts, i,
							src_size);
		diff_num += child_size;
		if (!child_size)
			goto cleanup_child;
		da_append_arr(res, child, child_size);
cleanup_child:
		da_free(child);
	}
	if (opts.progress_bar && s->ur) {
		snprintf(progress_desc, sizeof(progress_desc), "diffusion tracks [%zu/%zu]", src_size, src_size);
		s->ur(s->update_entry, diff_num, diff_num, progress_desc);
	}

	if (opts.progress_bar && s->uc) {
		s->uc(s->update_entry);
	}
	if (_lastfmapi_latracks_score(res, da_len(res), opts.lastfmapi_opts.diff_lambda, period,
				      opts.lastfmapi_opts.score_beta) < 0) {
		da_free(res);
		return;
	}
	qsort(res, da_len(res), sizeof(*res), la_track_score_cmp);
#ifdef LASTFMAPI_DEBUG
	fprintf(stderr, "res of diff core\n");
	for (i = 0; i < da_len(res); ++i) {
		la_track_dump(stderr, res[i]);
	}
#endif

	da_init(pl, sizeof(playitem));
	assert(diff_num == da_len(res));
	diff_num = target < diff_num ? target : diff_num;
	for (i = 0; i < diff_num; ++i) {
		lastfmapi_latrack2playitem(res[i], &pi);
		da_append(pl, pi);
	}
	for (i = 0; i < da_len(res); ++i) {
		la_track_free(res[i]);
	}
	da_free(res);
	*p_ref = pl;
}

LASTFMAPI_DECL
size_t _lastfmapi_diffusion_track(la_track *info, int diffusion, size_t target, lastfmapi_diffusion_strategy strategy,
				  const la_track *map, la_track ***res, lastfmapi_source *ls, recomm_option opts,
				  size_t cur_idx, size_t total_tracks)
{
	df_deque dq;
	la_track *cur, *find, *str, *sar, **sim_trs, **sim_ars, **total_res;
	size_t i, sim_tr_size, sim_ar_size, diff;
	size_t diff_size = 0;
	char progress_desc[256];

	if (strategy & LASTFMAPI_DIFFUSION_DFS)
		dq = df_stack;
	else if (strategy & LASTFMAPI_DIFFUSION_BFS)
		dq = df_queue;
	assert(dq.push && dq.pop);
	df_deque_init(&dq, target);
	dq.push(&dq, info);
	total_res = NULL;
	da_init(total_res, sizeof(*total_res));
	diff = opts.lastfmapi_opts.diff_size;

	source *s = &ls->src;
	static size_t cur_size = 0;
	while (!df_deque_empty(&dq)) {
#ifdef LASTFMAPI_DEBUG
		fprintf(stderr, "current dq size:%zu\n", dq.size);
#endif
		size_t _diff_size = 0;
		cur = dq.pop(&dq);
		++cur_size;
#ifdef LASTFMAPI_DEBUG
		fprintf(stderr, "curent ltr diffusion is %d\n", cur->diffusion);
#endif
		if (cur->diffusion >= diffusion) {
			goto cleanup;
		}
		sim_tr_size = _lastfmapi_get_track_similar(cur, diff, &sim_trs, ls, opts);
		if (!sim_tr_size && !sim_trs)
			goto after_track;
		for (i = 0; i < sim_tr_size; ++i) {
			/*
			 * ltr in recent track will be kept in diffusion path
			 * but will not be appended into result.
			 */
			str = sim_trs[i];
			dq.push(&dq, str);
			HASH_FIND(hh, map, &str->key, sizeof(struct tr_key_t), find);
			if (find == NULL) {
				da_append(total_res, str);
				str->in_result = true;
				++_diff_size;
			} else {
				str->ruts = find->ruts;
			}
		}
		diff_size += _diff_size;
		xfree(sim_trs);
after_track:
		_diff_size = 0;
		sim_ar_size = _lastfmapi_diffusion_artist(cur, diffusion, diff, target, map, &sim_ars, ls, opts);
		if (!sim_ar_size && !sim_ars)
			goto cleanup;
		for (i = 0; i < sim_ar_size; ++i) {
			sar = sim_ars[i];
			dq.push(&dq, sar);
			HASH_FIND(hh, map, &sar->key, sizeof(struct tr_key_t), find);
			if (find == NULL) {
				da_append(total_res, sar);
				sar->in_result = true;
				++_diff_size;
			} else {
				sar->ruts = find->ruts;
			}
		}
		diff_size += _diff_size;
		xfree(sim_ars);
cleanup:
		if (opts.progress_bar && s->ur) {
			snprintf(progress_desc, sizeof(progress_desc), "diffusion tracks [%zu/%zu]", cur_idx,
				 total_tracks);
			s->ur(s->update_entry, cur_size, cur_size + dq.size, progress_desc);
		}
		if (cur->internal && !cur->in_result) {
			la_track_free(cur);
		}
	}
	df_deque_free(&dq);
	*res = total_res;
	return diff_size;
}

LASTFMAPI_DECL
la_track *lastfmapi_json2similar_latrack(yyjson_val *val, bool strict, struct json_err *jerr)
{
	la_track *ltr;
	yyjson_val *jar;
	const char *name, *ar, *tr_mbid, *ar_mbid;
	double match;

	name = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (name == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	match = yyjson_get_num(yyjson_obj_get(val, "match"));

	jar = yyjson_obj_get(val, "artist");
	ar = yyjson_get_str(yyjson_obj_get(jar, "name"));
	if (ar == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	tr_mbid = yyjson_get_str(yyjson_obj_get(val, "mbid"));
	ar_mbid = yyjson_get_str(yyjson_obj_get(jar, "mbid"));

	ltr = la_track_new(name, ar, tr_mbid, ar_mbid);
	ltr->ruts = 0;
	ltr->match = match;
	ltr->internal = true;
	ltr->in_result = false;
cleanup:
	return ltr;
}

LASTFMAPI_DECL
size_t _lastfmapi_get_track_similar(la_track *ltr, unsigned diff_size, la_track ***res, lastfmapi_source *ls,
				    recomm_option opts)
{
	size_t i, track_size;
	la_track **_res = NULL;
	yyjson_read_err err;
	yyjson_doc *doc = NULL;
	yyjson_val *root, *tracks, *track;
	struct json_err jerr;
	jerr.length = 0;
	curlbuf buf;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	char diff_size_s[LONG_STR_SIZE];
	ull2string(diff_size_s, LONG_STR_SIZE, diff_size);

	if (lastfmapi_curl(&buf, ls, false, LASTFMAPI_TRACK_GETSIMILAR, 6, MAKE_KV("artist", (char *)ltr->key.artist),
			   MAKE_KV("track", (char *)ltr->key.name), MAKE_KV("mbid", (char *)ltr->key.tr_mbid),
			   MAKE_KV("api_key", ls->key), MAKE_KV("limit", diff_size_s),
			   MAKE_KV("format", LASTFMAPI_FORMAT)) < 0) {
		mxrec_cleanup(cleanup, track_size, 0);
	}
	doc = yyjson_read_opts(buf.buf, buf.len, 0, NULL, &err);
	if (doc == NULL) {
		write_jsonerr(&jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n", JSON_ERR_HEAD,
			      err.msg, err.code, err.pos);
		_res = NULL;
		mxrec_cleanup(cleanup, track_size, 0);
	}

	root = yyjson_doc_get_root(doc);

	tracks = yyjson_obj_get(yyjson_obj_get(root, "similartracks"), "track");
	// track.getsimilar will return 200 OK HTTP status code on error
	if (!yyjson_is_arr(tracks)) {
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
		_res[i] = lastfmapi_json2similar_latrack(track, opts.strict, &jerr);
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
la_artist _lastfmapi_json2sim_artist(yyjson_val *val, bool strict, struct json_err *jerr)
{
	la_artist lar = {0};
	const char *name, *mbid;
	double match;
	const char *match_s;

	name = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (name == NULL)
		write_jsonerr(jerr, "failed to get field \"name\" of artist");

	mbid = yyjson_get_str(yyjson_obj_get(val, "mbid"));
	if (mbid == NULL)
		write_jsonerr(jerr, "failed to get field \"mbid\" of artist");

	match_s = yyjson_get_str(yyjson_obj_get(val, "match"));
	if (string2d(match_s, MAX_DOUBLE_CHARS, &match) == 0)
		match = 0;

	la_artist_init(&lar, name, mbid, match);
	return lar;
}

LASTFMAPI_DECL
size_t _lastfmapi_jsonbuf2sim_artists(curlbuf *buf, unsigned diffusion, bool strict, la_artist **res,
				      struct json_err *jerr)
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
		_res[i].diffusion = diffusion + 1;
	}

cleanup:
	*res = _res;
	yyjson_doc_free(doc);
	return ar_size;
}

LASTFMAPI_DECL
la_track *_lastfmapi_json2top_track(yyjson_val *val, bool strict, struct json_err *jerr)
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

	ltr = la_track_new(name, artist, tr_mbid, ar_mbid);
	ltr->internal = ltr->in_result = false;
cleanup:
	return ltr;
}

LASTFMAPI_DECL
size_t _lastfmapi_artist_top_tracks(const la_artist *ar, unsigned diff_size, la_track *parent, la_track ***res,
				    lastfmapi_source *ls, recomm_option opts, struct json_err *jerr)
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
			   MAKE_KV("format", LASTFMAPI_FORMAT)) < 0) {
		_res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	doc = yyjson_read_opts(buf.buf, buf.len, 0, 0, &err);
	if (doc == NULL) {
		write_jsonerr(&jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n", JSON_ERR_HEAD,
			      err.msg, err.code, err.pos);
		_res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	root = yyjson_doc_get_root(doc);
	tracks = yyjson_obj_get(yyjson_obj_get(root, "toptracks"), "track");

	// artist.gettoptracks will return 200 OK HTTP status code on error
	if (!yyjson_is_arr(tracks)) {
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
		_res[i] = _lastfmapi_json2top_track(tr, opts.strict, jerr);
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
					 la_track ***res, lastfmapi_source *ls, recomm_option opts,
					 struct json_err *jerr)
{
	size_t i, total_size, tr_size;
	la_track **trs, **total_res = NULL, **_res;
	da_init(total_res, sizeof(*total_res));

	for (i = 0; i < ar_size; ++i) {
		tr_size = _lastfmapi_artist_top_tracks(&ars[i], diff_size, parent, &trs, ls, opts, jerr);
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
size_t _lastfmapi_diffusion_artist(la_track *ltr, int diffusion, unsigned diff_size, size_t target, const la_track *map,
				   la_track ***res, lastfmapi_source *ls, recomm_option opts)
{
	size_t i, ar_size = 0, tr_size;
	la_artist *ars = NULL;
	u8s artist = NULL;
	curlbuf buf;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	char diff_size_s[LONG_STR_SIZE];
	diff_size = (unsigned)sqrt(diff_size);
	ull2string(diff_size_s, LONG_STR_SIZE, diff_size);
	struct json_err jerr;
	jerr.length = 0;

	if (ltr->diffusion >= diffusion - 1) {
		*res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	// here simply get the first artist
	artist = _lastfmapi_artist_first(ltr);
	if (lastfmapi_curl(&buf, ls, false, LASTFMAPI_ARTIST_GETSIMILAR, 5, MAKE_KV("artist", (char *)artist),
			   MAKE_KV("mbid", (char *)ltr->key.ar_mbid), MAKE_KV("api_key", ls->key),
			   MAKE_KV("limit", diff_size_s), MAKE_KV("format", LASTFMAPI_FORMAT)) < 0) {
		*res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	ar_size = _lastfmapi_jsonbuf2sim_artists(&buf, ltr->diffusion, opts.strict, &ars, &jerr);
	if (!ar_size && !ars) {
		*res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	tr_size = _lastfmapi_all_artists_top_tracks(ars, ar_size, diff_size, ltr, res, ls, opts, &jerr);

cleanup:
	u8sfree(artist);
	for (i = 0; i < ar_size; ++i) {
		la_artist_cleanup(&ars[i]);
	}
	xfree(ars);
	curlbuf_free(&buf);
	return tr_size;
}

LASTFMAPI_DECL
double _ruts_curve(time_t ruts, time_t period, double beta)
{
	if (period <= 0)
		return (ruts == 0) ? 1 : 0;
	double x = (double)ruts / period;
	return 1.0 - log(1.0 + beta * x) / log(1.0 + beta);
}

#define DEFAULT_SCORE 100.0f
LASTFMAPI_DECL
int _lastfmapi_latrack_score(la_track *ltr, double lambda, time_t period, double beta)
{
	if (ltr == NULL)
		return 0;
	la_track *cur;
	cur = ltr;
	double factor, w, numerator, denominator;
	numerator = denominator = 0.0;
	while (cur) {
		w = exp(-lambda * _ruts_curve(cur->ruts, period, beta));
		numerator += cur->match * w;
		denominator += w;
		cur = cur->parent;
	}
	if (fabs(denominator) < 1e-12) {
		factor = 0;

	} else {
		factor = numerator / denominator;
	}
	ltr->score = DEFAULT_SCORE * factor;
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
int lastfmapi_check_opts(recomm_option opts, char **msg)
{
	int ret = 0;
	StrBuffer sb;
	sb_init(&sb, 128);
	if (fabs(opts.lastfmapi_opts.random_lambda) < 1e-12) {
		sb_append_str(&sb, "lastfmapi's random lambda must be positive.\n");
		ret = -1;
	}
	if (fabs(opts.lastfmapi_opts.diff_lambda) < 1e-12) {
		sb_append_str(&sb, "lastfmapi's diff lambda must be positive.\n");
		ret = -1;
	}
	if (fabs(opts.lastfmapi_opts.score_beta) < 1e-12) {
		sb_append_str(&sb, "lastfmapi's score beta must be positive.\n");
		ret = -1;
	}
	if (msg && sb.len > 0)
		*msg = xstrndup(sb.buf, sb.len + 1);
	sb_free(&sb);
	return ret;
}

// interface implement
LASTFMAPI_DECL
void lastfmapi_source_destroy(void *sp)
{
	lastfmapi_source *s = (lastfmapi_source *)sp;
	lastfm_security_free();
	u8sfree(s->username);
	xfree(s->base_url);
	xfree(s->key);
	xfree(s->period);
	curl_easy_cleanup(s->curl);
}

LASTFMAPI_DECL
int lastfmapi_recomm_single(source *s, playitem *p, recomm_option opts)
{
	panic("lastfm-api do not support single recommendation.");
}

LASTFMAPI_DECL
int lastfmapi_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	mxrec_log("Calling lastfm-api recomm_multi\n");
	int ret;
	curlbuf buf;
	la_track *la_tr_map = NULL;
	time_t period;
	struct json_err jerr;
	jerr.length = 0;
	char *opt_msg = NULL;

	if (lastfmapi_check_opts(opts, &opt_msg) < 0) {
		error("recomm_option error setted:%s", opt_msg);
		xfree(opt_msg);
		return -1;
	}

	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	assert(p);
	if (opts.level == RECOMM_FULL) {
		panic("lastfm api source don't support full level recommendation in recomm_multi");
	}

	lastfmapi_source *ls = (lastfmapi_source *)s;

	period = string2timestamp(ls->period);
	char from_s[LONG_STR_SIZE];
	size_t str_len = ull2string(from_s, LONG_STR_SIZE, period);

	if (str_len == 0)
		mxrec_cleanup(cleanup, ret, -1);

	from_s[str_len] = '\0';

	if ((ret = lastfmapi_curl(&buf, ls, opts.progress_bar, LASTFMAPI_USER_GETRECENTTRACKS, 4,
				  MAKE_KV("username", (char *)ls->username), MAKE_KV("api_key", ls->key),
				  MAKE_KV("from", from_s), MAKE_KV("format", LASTFMAPI_FORMAT)) < 0))
		mxrec_cleanup(cleanup, ret, ret);

	// recent tracks map
	if ((ret = lastfmapi_jsonbuf2recent_latrack_map(&buf, opts.strict, &la_tr_map, &jerr)) < 0) {
		print_jsonerr(&jerr);
		mxrec_cleanup(cleanup, ret, ret);
	}
	curlbuf_clear(&buf);

	ret = lastfmapi_diffusion_recomm(ls, opts, la_tr_map, p, period, num);

cleanup:
	la_track_map_cleanup(la_tr_map);
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
		error("impersonate failed with profile %s:\'%s\', using curl without security", lsc->profile,
		      curl_easy_strerror(code));
		curl_easy_reset(curl);
	}
}

#define check(expr, val)                                                                                               \
	do {                                                                                                           \
		if (!(expr)) {                                                                                         \
			(val) = false;                                                                                 \
			error("lastfm-web source init failed on %s", (#expr));                                         \
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
	check(!(ls->strategy & LASTFMAPI_DIFFUSION_ERR), ret);
	return ret;
}
