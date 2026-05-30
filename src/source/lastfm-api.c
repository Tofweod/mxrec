#include "lastfm-api.h"
#include "bb.h"
#include "comm.h"
#include "config.h"
#include "curl-impersonate.h"
#include "da.h"
#include "json.h"
#include "lastfm-comm.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/comm-source.h"
#include "u8string.h"
#include "uthash.h"
#include "utils/string.h"
#include "xmalloc.h"
#include "yyjson/src/yyjson.h"
#include <curl/curl.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define LASTFMAPI_DEBUG

#define LASTFMAPI_DECL static

#define JSON_ERR_HEAD "LASTFMWEB"

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
time_t lastfmapi_now(void)
{
	static time_t now_time = 0;
	if (now_time == 0)
		now_time = time(NULL);
	return now_time;
}

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
	fprintf(stderr, "recent hash map is\n");
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
int lastfmapi_latrack2playitem()
{
	// TODO
}

// core: diffusion recommendation recursively
typedef enum diffusion_strategy {
	DIFFUSION_BFS,
	DIFFUSION_DFS,
} diffusion_strategy;

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
diffusion_strategy str2strategy(const char *s)
{
	const char *err_strategy;
	if (s == NULL)
		mxrec_cleanup(err, err_strategy, "NULL");
	if (strcmp(s, "bfs") == 0)
		return DIFFUSION_BFS;
	else if (strcmp(s, "dfs") == 0)
		return DIFFUSION_DFS;
	else
		mxrec_cleanup(err, err_strategy, s);
err:
	error("failed to convert %s into diffusion_strategy,"
	      "using default dfs",
	      err_strategy);
	return DIFFUSION_DFS;
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
size_t _lastfmapi_get_track_similar(la_track *ltr, unsigned diff_size, la_track ***res, lastfmapi_source *ls,
				    recomm_option opts);
LASTFMAPI_DECL
size_t _lastfmapi_diffusion_artist(la_track *info, int diffusion, unsigned diff, size_t target, size_t *cur_num,
				   const la_track *map, la_track ***res, lastfmapi_source *ls, recomm_option opts);

LASTFMAPI_DECL
size_t _lastfmapi_diffusion_track(la_track *info, int diffusion, size_t target, size_t *cur_num,
				  diffusion_strategy strategy, const la_track *map, la_track ***res,
				  lastfmapi_source *ls, recomm_option opts);

LASTFMAPI_DECL
void _lastfmapi_diffusion_core(diffusion_strategy strategy, int diffusion, time_t period, size_t target,
			       size_t *cur_num, la_track *map, playlist *p, lastfmapi_source *ls, recomm_option opts)
{
	size_t i, child_size;
	la_track *cur, **res, **child;
	res = NULL;
	da_init(res, sizeof(*res));
	// TODO Top N diffusion or RANDOM
	for (i = 0, cur = map; cur && i < target; cur = cur->hh.next, ++i) {
		child_size =
			_lastfmapi_diffusion_track(cur, diffusion, target, cur_num, strategy, map, &child, ls, opts);
		if (!child_size)
			goto cleanup_child;
		da_append_arr(res, child, child_size);
cleanup_child:
		da_free(child);
	}
#ifdef LASTFMAPI_DEBUG
	fprintf(stderr, "res of diff core\n");
	for (i = 0; i < da_len(res); ++i) {
		la_track_dump(stderr, res[i]);
	}
#endif
	// TODO score
	// TODO transform into playlist
	for (i = 0; i < da_len(res); ++i) {
		la_track_free(res[i]);
	}
	da_free(res);
}

LASTFMAPI_DECL
int lastfmapi_diffusion_recomm(lastfmapi_source *ls, recomm_option opts, diffusion_strategy strategy, la_track *map,
			       playlist *p, int diffusion, time_t period, size_t target)
{
	size_t cur_num = 0;
#ifdef LASTFMAPI_DEBUG
	fprintf(stderr, "origin hash map size is %u\n", HASH_CNT(hh, map));
#endif
	_lastfmapi_diffusion_core(strategy, diffusion, period, target, &cur_num, map, p, ls, opts);
	return cur_num;
}

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

	now = lastfmapi_now();
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
	.destroy = lastfmapi_source_destroy,
	.rsp = lastfmapi_recomm_single,
	.rmp = lastfmapi_recomm_multi,
	.sh = lastfmapi_security_handle,
	.cc = lastfmapi_config_check,
	.security = &__lastfm_security,
};

struct lastfmapi_source {
	source src;
	CURL *curl;

	u8s username;
	char *base_url;
	char *key;
	char *period;
	/**
	 * Diffusion should be positive, representing the maximum diffusion level.
	 */
	unsigned diffusion;
	unsigned diff_size;
	diffusion_strategy strategy;
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
	}
}

LASTFMAPI_DECL
int lastfmapi_curl(curlbuf *buf, CURL *curl, const char *base_url, const char *method, unsigned para_count, ...)
{
	int ret;
	CURLcode code;
	long http_code = 0;
	va_list paras;
	char *url;

#ifdef LASTFMAPI_DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
#endif

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfmapi_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

	va_start(paras, para_count);
	url = lastfmapi_parse_url(base_url, method, para_count, paras);
	va_end(paras);
	if (url == NULL)
		mxrec_cleanup(cleanup, ret, -1);
	curl_easy_setopt(curl, CURLOPT_URL, url);

	code = curl_easy_perform(curl);
	if (code != CURLE_OK)
		mxrec_cleanup(cleanup, ret, -1);

	curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &http_code);
	if (http_code != 200)
		mxrec_cleanup(err, ret, -2);

	ret = 0;
cleanup:
	xfree(url);
	return ret;
err:
	lastfmapi_curl_error(stderr, http_code, buf);
	return ret;
}

LASTFMAPI_DECL
int lastfmapi_source_init(void *sp, config_t *cfg)
{
	globalCurlInit();
	lastfmapi_source *s = (lastfmapi_source *)sp;
	s->src = __lastfmapi_source;

	if (lastfm_security_init(s->src.security, cfg) < 0)
		return -1;

	s->curl = curl_easy_init();
	if (s->curl == NULL)
		return -1;
	source_perform_security(s);

	s->username = u8sdup(cfg->lastfm_username);
	s->base_url = xstrdup(cfg->lastfmapi_base_url);
	s->key = xstrdup(cfg->lastfmapi_key);
	s->period = xstrdup(cfg->lastfmapi_period);
	s->diffusion = cfg->lastfmapi_diffusion_level;
	assert(s->diffusion > 0);
	s->diff_size = cfg->lastfmapi_diffusion_size;
	s->strategy = str2strategy(cfg->lastfmapi_strategy);
	return source_check(s);
}

extern int lastfmapi_source_new(source **src, config_t *cfg)
{
	assert(cfg);
	*src = NULL;
	void *s = xmalloc(sizeof(struct lastfmapi_source));
	if (!lastfmapi_source_init(s, cfg)) {
		xfree(s);
		return -1;
	}
	*src = s;
	return 0;
}

LASTFMAPI_DECL
size_t _lastfmapi_diffusion_track(la_track *info, int diffusion, size_t target, size_t *cur_num,
				  diffusion_strategy strategy, const la_track *map, la_track ***res,
				  lastfmapi_source *ls, recomm_option opts)
{
	df_deque dq;
	la_track *cur, *find, *str, *sar, **sim_trs, **sim_ars, **total_res;
	size_t i, sim_tr_size, sim_ar_size, diff_size = 0, diff;

	if (strategy == DIFFUSION_DFS)
		dq = df_stack;
	else
		dq = df_queue;
	df_deque_init(&dq, target);
	dq.push(&dq, info);
	total_res = NULL;
	da_init(total_res, sizeof(*total_res));
	diff = ls->diff_size;

	while (!df_deque_empty(&dq)) {
#ifdef LASTFMAPI_DEBUG
		fprintf(stderr, "current dq size:%zu\n", dq.size);
#endif
		size_t _diff_size = 0;
		cur = dq.pop(&dq);
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
			}
		}
		diff_size += _diff_size;
		*cur_num += _diff_size;
		xfree(sim_trs);
after_track:
		_diff_size = 0;
		sim_ar_size =
			_lastfmapi_diffusion_artist(cur, diffusion, diff, target, cur_num, map, &sim_ars, ls, opts);
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
			}
		}
		diff_size += _diff_size;
		*cur_num += _diff_size;
		xfree(sim_ars);
cleanup:
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
	char diff_size_s[sizeof(diff_size) + 1];
	ull2string(diff_size_s, sizeof(diff_size) + 1, diff_size);

	if (lastfmapi_curl(&buf, ls->curl, ls->base_url, LASTFMAPI_TRACK_GETSIMILAR, 6,
			   MAKE_KV("artist", (char *)ltr->key.artist), MAKE_KV("track", (char *)ltr->key.name),
			   MAKE_KV("mbid", (char *)ltr->key.tr_mbid), MAKE_KV("api_key", ls->key),
			   MAKE_KV("limit", diff_size_s), MAKE_KV("format", LASTFMAPI_FORMAT)) < 0) {
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
	// truncate size to 10
	if (string2d(match_s, 10, &match) == 0)
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
la_track *_lastfm_json2top_track(yyjson_val *val, bool strict, struct json_err *jerr)
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
	// TODO
	size_t i, tr_size;
	la_track **_res = NULL;
	char diff_size_s[sizeof(diff_size) + 1];
	ull2string(diff_size_s, sizeof(diff_size) + 1, diff_size);
	curlbuf buf;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);

	yyjson_doc *doc = NULL;
	yyjson_read_err err;
	yyjson_val *root, *tracks, *tr;

	if (lastfmapi_curl(&buf, ls->curl, ls->base_url, LASTFMAPI_ARTIST_GETTOPTRACKS, 5,
			   MAKE_KV("artist", (char *)ar->name), MAKE_KV("mbid", ar->mbid), MAKE_KV("api_key", ls->key),
			   MAKE_KV("limit", diff_size_s), MAKE_KV("format", LASTFMAPI_FORMAT)) < 0) {
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
		_res[i] = _lastfm_json2top_track(tr, opts.strict, jerr);
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
size_t _lastfmapi_diffusion_artist(la_track *ltr, int diffusion, unsigned diff_size, size_t target, size_t *cur_num,
				   const la_track *map, la_track ***res, lastfmapi_source *ls, recomm_option opts)
{
	size_t i, ar_size = 0, tr_size;
	la_artist *ars = NULL;
	u8s artist = NULL;
	curlbuf buf;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	char diff_size_s[sizeof(diff_size) + 1];
	diff_size = (unsigned)sqrt(diff_size);
	ull2string(diff_size_s, sizeof(diff_size) + 1, diff_size);
	struct json_err jerr;
	jerr.length = 0;

	if (ltr->diffusion >= diffusion - 1) {
		*res = NULL;
		mxrec_cleanup(cleanup, tr_size, 0);
	}

	// here simply get the first artist
	artist = _lastfmapi_artist_first(ltr);
	if (lastfmapi_curl(&buf, ls->curl, ls->base_url, LASTFMAPI_ARTIST_GETSIMILAR, 5,
			   MAKE_KV("artist", (char *)artist), MAKE_KV("mbid", (char *)ltr->key.ar_mbid),
			   MAKE_KV("api_key", ls->key), MAKE_KV("limit", diff_size_s),
			   MAKE_KV("format", LASTFMAPI_FORMAT)) < 0) {
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

// interface implement
LASTFMAPI_DECL
void lastfmapi_source_destroy(void *sp)
{
	lastfmapi_source *s = (lastfmapi_source *)sp;
	lastfm_security_free(s->src.security);
	u8sfree(s->username);
	xfree(s->base_url);
	xfree(s->key);
	xfree(s->period);
	curl_easy_cleanup(s->curl);
}

LASTFMAPI_DECL
int lastfmapi_recomm_single(source *s, playitem *p, recomm_option opts)
{
	// TODO
	return 0;
}

LASTFMAPI_DECL
int lastfmapi_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	log("Calling lastfm-api recomm_multi\n");
	int ret;
	curlbuf buf;
	la_track *la_tr_map = NULL;
	time_t period;
	struct json_err jerr;
	jerr.length = 0;

	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	assert(p);
	if (opts.level == RECOMM_FULL) {
		panic("lastfm api source don't support full level recommendation in recomm_multi");
	}

	lastfmapi_source *ls = (lastfmapi_source *)s;

	// TODO get period, here simply use limit for debug
	size_t limit = num;
	char limit_str[sizeof(limit) + 1];
	size_t str_len = ull2string(limit_str, sizeof(limit) + 1, limit);

	if (str_len == 0)
		mxrec_cleanup(cleanup, ret, -1);

	limit_str[str_len] = '\0';

	if ((ret = lastfmapi_curl(&buf, ls->curl, ls->base_url, LASTFMAPI_USER_GETRECENTTRACKS, 4,
				  MAKE_KV("username", (char *)ls->username), MAKE_KV("api_key", ls->key),
				  MAKE_KV("limit", limit_str), MAKE_KV("format", LASTFMAPI_FORMAT)) < 0))
		mxrec_cleanup(cleanup, ret, ret);

	// recent tracks map
	if ((ret = lastfmapi_jsonbuf2recent_latrack_map(&buf, opts.strict, &la_tr_map, &jerr)) < 0) {
		print_jsonerr(&jerr);
		mxrec_cleanup(cleanup, ret, ret);
	}
	curlbuf_clear(&buf);

	ret = lastfmapi_diffusion_recomm(ls, opts, ls->strategy, la_tr_map, p, ls->diffusion, period, num);

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
	check(ls->diffusion > 0, ret);
	check(ls->diff_size > 0, ret);
	return ret;
}
