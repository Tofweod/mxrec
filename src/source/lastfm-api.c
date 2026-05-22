#include "lastfm-api.h"
#include "assert.h"
#include "bb.h"
#include "comm.h"
#include "config.h"
#include "json.h"
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

#define LASTFMAPI_DECL static

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
	struct {
		u8s name;
		u8s artist;
		char *ar_mbid;
	} tr;
	UT_hash_handle hh;
	time_t ruts;
} la_track;

// la_track json parsing
int lastfmapi_jsonbuf2latrack_map(curlbuf *buf, la_track **la_tr_map, struct json_err *jerr)
{
	// TODO
}

static source __lastfmapi_source = {
	.destroy = lastfmapi_source_destroy,
	.rsp = lastfmapi_recomm_single,
	.rmp = lastfmapi_recomm_multi,
	.sh = lastfmapi_security_handle,
	.security = 0,
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
int lastfmapi_curl(curlbuf *buf, CURL *curl, bool security, const char *base_url,
		   const char *method, unsigned para_count, ...)
{
	int ret;
	CURLcode code;
	long http_code = 0;
	va_list paras;
	char *url;
	if (security) {
		// TODO
	}

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
		mxrec_cleanup(cleanup, ret, -1);

	ret = 0;
cleanup:
	xfree(url);
	curl_easy_reset(curl);
	return ret;
}

LASTFMAPI_DECL
int lastfmapi_source_init(void *sp, config_t *cfg)
{
	globalCurlInit();
	lastfmapi_source *s = (lastfmapi_source *)sp;
	s->src = __lastfmapi_source;

	s->curl = curl_easy_init();
	if (s->curl == NULL)
		return -1;

	// TODO
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
	// TODO
	lastfmapi_source *s = (lastfmapi_source *)sp;
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
	la_track *la_tr_map;
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

	if ((ret = lastfmapi_curl(&buf, ls->curl, opts.use_security,
				  ls->base_url,
				  LASTFMAPI_USER_GETRECENTTRACKS, 4,
				  MAKE_KV("username", (char *)ls->username),
				  MAKE_KV("api_key", ls->key),
				  MAKE_KV("limit", limit_str),
				  MAKE_KV("format", LASTFMAPI_FORMAT)) < 0))
		mxrec_cleanup(cleanup, ret, ret);

	// lastfm_track map
	if ((ret = lastfmapi_jsonbuf2latrack_map(&buf, &la_tr_map,
						 &jerr)) < 0) {
		print_jsonerr(&jerr);
		mxrec_cleanup(cleanup, ret, ret);
	}

	curlbuf_clear(&buf);

	// TODO diffusion recommendation

cleanup:
	curlbuf_free(&buf);
	return ret;
}

LASTFMAPI_DECL
void lastfmapi_security_handle(source *s)
{
	(void)s;
}
