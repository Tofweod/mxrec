#include "lastfm.h"
#include "artist.h"
#include "assert.h"
#include "bb.h"
#include "comm.h"
#include "config.h"
#include "curl-impersonate.h"
#include "da.h"
#include "json.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/comm-source.h"
#include "track.h"
#include "utils/string.h"
#include "xmalloc.h"
#include "yyjson/src/yyjson.h"
#include <stddef.h>

#define LASTFM_DECL static

#define BUF_DEFAULT_CAP (1024)

// TODO  bufclear will be used in retry-implementation
BUFFERBUILDER_INIT(LASTFM_DECL, curlbuf, curlbuf, void);

typedef struct lastfm_security {
	char *profile;
} lastfm_security;

LASTFM_DECL
int _securityinit(lastfm_security *security, config_t *cfg)
{
	security->profile = xstrdup(cfg->lastfm_security_profile);
	return 0;
}

LASTFM_DECL
void _securityfree(lastfm_security *security)
{
	xfree(security->profile);
}

static lastfm_security __security;

#define FUNCTION_FIELD(retype, name, ...) LASTFM_DECL retype lastfm_##name(__VA_ARGS__);

FUNCTION_FIELD_LIST

#undef FUNCTION_FIELD

static source __lastfm_source = {
	.destroy = lastfm_source_destroy,
	.rsp = lastfm_recomm_single,
	.rmp = lastfm_recomm_multi,
	.sh = lastfm_security_handle,
	.security = &__security,
};

struct lastfm_source {
	source src;
	CURL *curl;

	config_t *cfg;
};

LASTFM_DECL
int lastfm_source_init(void *sp, config_t *cfg)
{
	globalCurlInit();
	lastfm_source *s = (lastfm_source *)sp;
	s->src = __lastfm_source;

	if (_securityinit(s->src.security, cfg) < 0)
		return -1;

	s->curl = curl_easy_init();

	if (s->curl == NULL)
		return -1;

	s->cfg = cfg;

	return 0;
}

int lastfm_source_new(source **src, config_t *cfg)
{
	assert(cfg);
	*src = NULL;
	void *s = xmalloc(sizeof(struct lastfm_source));
	if (lastfm_source_init(s, cfg) < 0) {
		xfree(s);
		return -1;
	}
	*src = s;
	return 0;
}

LASTFM_DECL
char *lastfm_parse_url(lastfm_source *s, const char *_path, const char *_parameter, ...)
{
	va_list paras;
	char *realurl, *base_url,
		*url = NULL, *path = NULL,
		*hout = NULL, *parameter = NULL;
	size_t urlen;
	int bufstrlen;
	CURLU *h = NULL;

	config_t *cfg = s->cfg;
	base_url = cfg->lastfm_base_url;
	// future: wrapper parseKVFormat with lastfm config map,
	// in order to automatically parse path
	path = parseKVFormat(_path,
			     MAKE_KV("username", (const char *)cfg->lastfm_username), KV_END);

	if (path == NULL)
		mxrec_cleanup(cleanup, realurl, NULL);

	urlen = strlen(base_url);
	url = xmalloc(urlen);

	while (1) {
		bufstrlen = snprintf(url, urlen, "%s%s", base_url, path);
		if (bufstrlen < 0)
			mxrec_cleanup(cleanup, realurl, NULL);

		if ((size_t)bufstrlen >= urlen) {
			urlen = ((size_t)bufstrlen) + 1;
			url = xrealloc(url, urlen);
			continue;
		}
		break;
	}

	va_start(paras, _parameter);
	if (_parameter)
		parameter = parsevFormat(_parameter, paras);
	va_end(paras);

	if (_parameter && parameter == NULL)
		mxrec_cleanup(cleanup, realurl, NULL);

	// esape parameter
	h = curl_url();
	curl_url_set(h, CURLUPART_URL, url, 0);
	curl_url_set(h, CURLUPART_QUERY, parameter, 0);
	curl_url_get(h, CURLUPART_URL, &hout, 0);
	realurl = xstrdup(hout);

cleanup:
	xfree(path);
	xfree(url);
	xfree(parameter);
	if (hout)
		curl_free(hout);
	if (h)
		curl_url_cleanup(h);
	return realurl;
}

// json parser
#define JSON_READ_ERR -1
#define JSON_PARSE_PLAYLIST_ERR -2
#define JSON_PARSE_TRACK_ERR -3
#define JSON_PARSE_URL_ERR -4
LASTFM_DECL
artist *lastfm_json2artist(yyjson_val *val, bool strict, struct json_err *err)
{
	artist *ar;
	const char *name;

	name = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (name == NULL) {
		write_jsonerr(err, "failed to parse field \"name\" "
				   "into string in artist.\n");
		mxrec_cleanup(cleanup, ar, 0);
	}

	// lastfm don't support alias
	ar = artist_new(name, 0, NULL);

cleanup:
	return ar;
}

LASTFM_DECL
track *lastfm_json2track(yyjson_val *val, bool strict, struct json_err *err)
{
	track *tr;
	const char *title, *album;
	size_t i, ar_size;
	artist **artists, *a;
	yyjson_val *ars, *ar;

	// name
	title = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (title == NULL) {
		write_jsonerr(err, "failed to parse field \"name\" "
				   "into string in playlist item.\n");
		mxrec_cleanup(cleanup, tr, 0);
	}

	// album
	album = yyjson_get_str(yyjson_obj_get(val, "primary_album"));
	if (strict && album == NULL) {
		write_jsonerr(err, "failed to get album information of %s\n.",
			      title);
		mxrec_cleanup(cleanup, tr, 0);
	}

	// aritsts
	ars = yyjson_obj_get(val, "artists");
	if (!yyjson_is_arr(ars)) {
		write_jsonerr(err, "artists field is not an array.\n");
		mxrec_cleanup(cleanup, tr, 0);
	}

	ar_size = yyjson_arr_size(ars);
	artists = xmalloc(ar_size * sizeof(artists[0]));
	yyjson_arr_foreach(ars, i, ar_size, ar)
	{
		a = lastfm_json2artist(ar, strict, err);
		if (strict && a == NULL) {
			write_jsonerr(err, "failed to get an artist information of %s\n",
				      title);
			mxrec_cleanup(cleanup, tr, 0);
		}
		artists[i] = a;
	}

	if (artists[0] == NULL) {
		write_jsonerr(err, "%s cann't get the artist information.", title);
		mxrec_cleanup(cleanup, tr, 0);
	}

	// lastfm don't support alias
	tr = track_new(title, album, ar_size, artists, 0, NULL);
cleanup:
	return tr;
}

LASTFM_DECL
int lastfm_json2playitem(yyjson_val *val, playitem *pi, bool strict, struct json_err *err)
{
	int ret;
	track *tr;

	yyjson_val *playlinks;
	yyjson_arr_iter iter;
	yyjson_val *playlink;
	const char *url;

	tr = lastfm_json2track(val, strict, err);
	if (tr == NULL)
		return JSON_PARSE_TRACK_ERR;
	pi->tr = tr;

	// add urls
	playlinks = yyjson_obj_get(val, "playlinks");
	if (!yyjson_is_arr(playlinks)) {
		if (strict) {
			write_jsonerr(err, "playlinks field is not an array.\n");
			write_jsonerr(err, "ignore url error of %s\n", tr->title);
		}
		mxrec_cleanup(cleanup, ret, JSON_PARSE_URL_ERR);
	}

	iter = yyjson_arr_iter_with(playlinks);
	while ((playlink = yyjson_arr_iter_next(&iter))) {
		url = yyjson_get_str(yyjson_obj_get(playlink, "url"));
		playitem_addurl(pi, url);
	}

	ret = 0;
cleanup:
	return ret;
}

LASTFM_DECL
int lastfm_json2playlist(curlbuf *buf, size_t wanted, playlist *p_ref, bool strict, struct json_err *jerr)
{
	int ret;
	size_t i, plen, handled = 0;
	yyjson_read_err err;
	yyjson_doc *doc;
	yyjson_val *root, *pls, *jpi;

	playitem pi;
	playlist pl;
	pl = NULL;
	da_init(pl, sizeof(playitem));

	doc = yyjson_read_opts(buf->buf, buf->len,
			       0, NULL, &err);

	if (doc == NULL) {
		write_jsonerr(jerr, "failed to parse json: %s, code: %u at byte position: %lu\n",
			      err.msg, err.code, err.pos);
		mxrec_cleanup(cleanup, ret, JSON_READ_ERR);
	}

	root = yyjson_doc_get_root(doc);
	pls = yyjson_obj_get(root, "playlist");
	if (!yyjson_is_arr(pls)) {
		write_jsonerr(jerr, "playlist field is not an array.\n");
		mxrec_cleanup(cleanup, ret, JSON_PARSE_PLAYLIST_ERR);
	}

	plen = yyjson_arr_size(pls);

	plen = wanted <= plen ? wanted : plen;

	yyjson_arr_foreach(pls, i, plen, jpi)
	{
		int pi_ret;
		playitem_init(&pi);
		pi_ret = lastfm_json2playitem(jpi, &pi, strict, jerr);

		if (strict && pi_ret < 0)
			mxrec_cleanup(cleanup, ret, pi_ret);
		else if (pi_ret == JSON_PARSE_TRACK_ERR)
			mxrec_cleanup(cleanup, ret, pi_ret);
		// ignore JSON_PARSE_URL_ERR when not strict
		da_append(pl, pi);
		handled++;
	}

	*p_ref = pl;
	ret = handled;
cleanup:
	yyjson_doc_free(doc);
	return ret;
}

// write back in curlbuf
LASTFM_DECL
size_t lastfm_write_callback(char *ptr, const size_t size, const size_t nmemb, curlbuf *cb)
{
	const size_t real = size * nmemb;
	curlbuf_append(cb, ptr, real);

	return real;
}

LASTFM_DECL
int _lastfm_recomm_single_full(source *s, playitem *p, recomm_option opts)
{
	// TODO
}

LASTFM_DECL
int _lastfm_recomm_single_simple(source *s, playitem *p, recomm_option opts)
{
	// TODO
}

LASTFM_DECL
void lastfm_source_destroy(void *sp)
{
	lastfm_source *s = (lastfm_source *)sp;
	_securityfree(s->src.security);
	curl_easy_cleanup(s->curl);
}

// interface implement
LASTFM_DECL
int lastfm_recomm_single(source *s, playitem *p, recomm_option opts)
{
	printf("Calling lastfm recomm_single\n");
	if (opts.use_security)
		perform_security(s);

	if (opts.level == RECOMM_SIMPLE)
		return _lastfm_recomm_single_simple(s, p, opts);
	else if (opts.level == RECOMM_FULL)
		return _lastfm_recomm_single_full(s, p, opts);
	else
		return -1;
}

/**
 * Fuck lastfm, the size of playlist captured from http request is not fixed.
 * So here if assigned `num` is smaller than playlist size it works normally,
 * else return playlist size.
 * Doing multi calls to meet `num` value is fucking silly.
 */
LASTFM_DECL
int lastfm_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	int ret;
	CURLcode code;
	curlbuf buf;
	struct json_err jerr;
	jerr.length = 0;
	long http_code = 0;
	assert(p);
	if (opts.level == RECOMM_FULL) {
		panic("lastfm source don't support full level recommendation");
	}
	assert(opts.level == RECOMM_SIMPLE);
	lastfm_source *ls = (lastfm_source *)s;
	config_t *cfg = ls->cfg;
	if (opts.use_security)
		perform_security(s);

	CURL *curl = ls->curl;
	struct curl_slist *h = NULL;

#if 0
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

	// set url
	char *realurl = lastfm_parse_url(ls,
					 cfg->lastfm_mrc_path,
					 cfg->lastfm_mrc_parameter, num);
	if (realurl == NULL)
		mxrec_cleanup(cleanup, ret, -1);

	curl_easy_setopt(curl, CURLOPT_URL, realurl);

	// set method
	if (strcmp(cfg->lastfm_mrc_method, "GET") == 0) {
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	} else if (strcmp(cfg->lastfm_mrc_method, "POST") == 0) {
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
	}

	// set headers
	h = curl_slist_append(h, cfg->lastfm_mrc_accept);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);

	// set write callback
	curlbuf_init(&buf, BUF_DEFAULT_CAP);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfm_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

	code = curl_easy_perform(curl);
	if (code != CURLE_OK)
		mxrec_cleanup(cleanup, ret, -1);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code != 200)
		mxrec_cleanup(cleanup, ret, -1);

	if ((ret = lastfm_json2playlist(&buf, num, p, opts.strict, &jerr)) < 0)
		mxrec_cleanup(cleanup, ret, -1);

cleanup:
	// TODO log jerr
	xfree(realurl);
	if (h)
		curl_slist_free_all(h);
	curlbuf_free(&buf);
	curl_easy_reset(curl);
	return ret;
}

LASTFM_DECL
void lastfm_security_handle(source *s)
{
	CURLcode code = CURLE_OK;
	lastfm_source *ls = (lastfm_source *)s;
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
