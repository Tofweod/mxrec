#include "lastfm-api.h"
#include "bb.h"
#include "comm.h"
#include "config.h"
#include "curl-impersonate.h"
#include "json.h"
#include "lastfm-security.h"
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
#include <time.h>

#define LASTFMAPI_DEBUG

#define LASTFMAPI_DECL static

#define JSON_ERR_HEAD "LASTFMWEB"

// curlbuf
#define BUF_DEFAULT_CAP (1024)
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

typedef struct lastfmapi_track {
	double score;
	struct tr_key_t {
		u8s name;
		u8s artist;
		char *ar_mbid;
	} key;
	time_t ruts;
	UT_hash_handle hh;
} la_track;

void la_track_free(la_track *ltr)
{
	if (ltr == NULL)
		return;
	u8sfree(ltr->key.name);
	u8sfree(ltr->key.artist);
	xfree(ltr->key.ar_mbid);
}

la_track *la_track_new(const char *name, const char *artist, const char *ar_mid,
		       unsigned long long uts)
{
	la_track *ltr;
	time_t now;
	ltr = xmalloc(sizeof(*ltr));
	ltr->key.name = u8snew(name);
	ltr->key.artist = u8snew(artist);
	ltr->key.ar_mbid = strdup(ar_mid);

	now = lastfmapi_now();

	assert(now >= uts);
	ltr->ruts = now - uts;
	return ltr;
}

void la_track_map_cleanup(la_track *m)
{
	la_track *cur, *tmp;
	HASH_ITER(hh, m, cur, tmp)
	{
#ifdef LASTFMAPI_DEBUG
		printf("name:%s-artist:%s-ar_mid:%s-%lu\n",
		       cur->key.name, cur->key.artist, cur->key.ar_mbid,
		       cur->ruts);
#endif
		HASH_DEL(m, cur);
		la_track_free(cur);
		xfree(cur);
	}
}

LASTFMAPI_DECL
int lastfmapi_latrack2playitem()
{
	// TODO
}

// core: diffusion recommendation recursively
LASTFMAPI_DECL
int lastfmapi_diffusion_recomm(la_track *map, playlist *p)
{
	// TODO
}

// la_track json parsing
la_track *lastfmapi_json2latrack(yyjson_val *val, bool strict, struct json_err *err)
{
	la_track *ltr;
	yyjson_val *jname, *jar;
	const char *name, *ar, *ar_mid, *uts_s;
	unsigned long long uts;

	jname = yyjson_obj_get(val, "name");
	name = yyjson_get_str(jname);
	if (name == NULL) {
		mxrec_cleanup(cleanup, ltr, 0);
	}

	jar = yyjson_obj_get(val, "artist");
	ar = yyjson_get_str(yyjson_obj_get(jar, "#text"));
	if (ar == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	ar_mid = yyjson_get_str(yyjson_obj_get(jar, "mbid"));
	if (ar_mid == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	uts_s = yyjson_get_str(yyjson_obj_get(
		yyjson_obj_get(val, "date"), "uts"));
	if (uts_s == NULL)
		mxrec_cleanup(cleanup, ltr, 0);

	if (!string2ull(uts_s, &uts))
		mxrec_cleanup(cleanup, ltr, 0);

	ltr = la_track_new(name, ar, ar_mid, uts);
cleanup:
	return ltr;
}

int lastfmapi_jsonbuf2latrack_map(curlbuf *buf, bool strict, la_track **la_tr_map, struct json_err *jerr)
{
	int ret;
	size_t i, track_size;
	yyjson_read_err err;
	yyjson_doc *doc;
	yyjson_val *root, *tracks, *track;
	la_track *m, *ltr;
	m = NULL;

	doc = yyjson_read_opts(buf->buf, buf->len,
			       0, NULL, &err);
	if (doc == NULL) {
		write_jsonerr(jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n",
			      JSON_ERR_HEAD, err.msg, err.code, err.pos);
		mxrec_cleanup(cleanup, ret, -1);
	}

	root = yyjson_doc_get_root(doc);
	tracks = yyjson_obj_get(yyjson_obj_get(root, "recenttracks"), "track");
	if (tracks == NULL || !yyjson_is_arr(tracks)) {
		write_jsonerr(jerr, "[%s]: failed to parse field \".recenttracks.track\" into array",
			      JSON_ERR_HEAD);
		mxrec_cleanup(cleanup, ret, -1);
	}

	track_size = yyjson_arr_size(tracks);

	yyjson_arr_foreach(tracks, i, track_size, track)
	{
		ltr = lastfmapi_json2latrack(track, strict, jerr);
		if (ltr == NULL) {
			if (strict) {
				write_jsonerr(jerr, "[%s]: failed to get track", JSON_ERR_HEAD);
				mxrec_cleanup(cleanup, ret, -1);
			}
			continue;
		}
		HASH_ADD(hh, m, key, sizeof(struct tr_key_t), ltr);
	}

	ret = 0;
cleanup:
	yyjson_doc_free(doc);
	*la_tr_map = m;
	return ret;
}

static source __lastfmapi_source = {
	.destroy = lastfmapi_source_destroy,
	.rsp = lastfmapi_recomm_single,
	.rmp = lastfmapi_recomm_multi,
	.sh = lastfmapi_security_handle,
	.security = &__lastfm_security,
};

struct lastfmapi_source {
	source src;
	CURL *curl;

	// TODO
	u8s username;
	char *base_url;
	char *key;
	char *period;
	int diffusion;
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
		curl_url_set(h, CURLUPART_QUERY, kv_str,
			     CURLU_APPENDQUERY);
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
int lastfmapi_curl(source *s, curlbuf *buf, CURL *curl, bool security, const char *base_url,
		   const char *method, unsigned para_count, ...)
{
	int ret;
	CURLcode code;
	long http_code = 0;
	va_list paras;
	char *url;
	if (security)
		perform_security(s);

#ifdef LASTFMAPI_DEBUG
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
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
	curl_easy_reset(curl);
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

	s->username = u8sdup(cfg->lastfm_username);
	s->base_url = xstrdup(cfg->lastfmapi_base_url);
	s->key = xstrdup(cfg->lastfmapi_key);
	s->period = xstrdup(cfg->lastfmapi_period);
	s->diffusion = cfg->lastfmapi_diffusion;
	return 0;
}

extern int lastfmapi_source_new(source **src, config_t *cfg)
{
	assert(cfg);
	*src = NULL;
	void *s = xmalloc(sizeof(struct lastfmapi_source));
	if (lastfmapi_source_init(s, cfg) < 0) {
		xfree(s);
		return -1;
	}
	*src = s;
	return 0;
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
}

LASTFMAPI_DECL
int lastfmapi_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	log("Calling lastfm-api recomm_multi\n");
	int ret;
	curlbuf buf;
	la_track *la_tr_map = NULL;
	struct json_err jerr;
	jerr.length = 0;

	curlbuf_init(&buf, BUF_DEFAULT_CAP);
	assert(p);
	if (opts.level == RECOMM_FULL) {
		panic("lastfm api source don't support full level recommendation in recomm_multi");
	}

	lastfmapi_source *ls = (lastfmapi_source *)s;

	// TODO get returned size from num
	size_t limit = num;
	char limit_str[sizeof(limit) + 1];
	size_t str_len = ull2string(limit_str, sizeof(limit) + 1, limit);

	if (str_len == 0)
		mxrec_cleanup(cleanup, ret, -1);

	limit_str[str_len] = '\0';

	if ((ret = lastfmapi_curl(s, &buf, ls->curl, opts.use_security,
				  ls->base_url,
				  LASTFMAPI_USER_GETRECENTTRACKS, 4,
				  MAKE_KV("username", (char *)ls->username),
				  MAKE_KV("api_key", ls->key),
				  MAKE_KV("limit", limit_str),
				  MAKE_KV("format", LASTFMAPI_FORMAT)) < 0))
		mxrec_cleanup(cleanup, ret, ret);

	// recent tracks map
	if ((ret = lastfmapi_jsonbuf2latrack_map(&buf, opts.strict, &la_tr_map,
						 &jerr)) < 0) {
		print_jsonerr(&jerr);
		mxrec_cleanup(cleanup, ret, ret);
	}
	curlbuf_clear(&buf);

	// TODO diffusion recommendation
	ret = lastfmapi_diffusion_recomm(la_tr_map, p);

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
