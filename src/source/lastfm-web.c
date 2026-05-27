#include "lastfm-web.h"
#include "artist.h"
#include "assert.h"
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
#include "track.h"
#include "utils/string.h"
#include "xmalloc.h"
#include "yyjson/src/yyjson.h"
#include <stddef.h>

#define LASTFMWEB_DECL static

#define BUF_DEFAULT_CAP (1024)

BUFFERBUILDER_INIT(mxrec_unused LASTFMWEB_DECL, curlbuf, curlbuf, void);

#define FUNCTION_FIELD(retype, name, ...) LASTFMWEB_DECL retype lastfmweb_##name(__VA_ARGS__);

FUNCTION_FIELD_LIST

#undef FUNCTION_FIELD

static source __lastfmweb_source = {
	.destroy = lastfmweb_source_destroy,
	.rsp = lastfmweb_recomm_single,
	.rmp = lastfmweb_recomm_multi,
	.sh = lastfmweb_security_handle,
	.cc = lastfmweb_config_check,
	.security = &__lastfm_security,
};

struct lastfmweb_source {
	source src;
	CURL *curl;

	char *base_url;
	u8s username;
	char *recomm_path;
	char *recomm_method;
	char *recomm_accept;
	char *recomm_parameter;
};

LASTFMWEB_DECL
int lastfmweb_source_init(void *sp, config_t *cfg)
{
	globalCurlInit();
	lastfmweb_source *s = (lastfmweb_source *)sp;
	s->src = __lastfmweb_source;

	if (lastfm_security_init(s->src.security, cfg) < 0)
		return -1;

	s->curl = curl_easy_init();

	if (s->curl == NULL)
		return -1;

	s->base_url = xstrdup(cfg->lastfmweb_base_url);
	s->username = u8sdup(cfg->lastfm_username);
	s->recomm_path = xstrdup(cfg->lastfmweb_recomm_path);
	s->recomm_method = xstrdup(cfg->lastfmweb_recomm_method);
	s->recomm_accept = xstrdup(cfg->lastfmweb_recomm_accept);
	s->recomm_parameter = xstrdup(cfg->lastfmweb_recomm_parameter);

	return source_check(s);
}

extern int lastfmweb_source_new(source **src, config_t *cfg)
{
	assert(cfg);
	*src = NULL;
	void *s = xmalloc(sizeof(struct lastfmweb_source));
	if (!lastfmweb_source_init(s, cfg)) {
		xfree(s);
		return -1;
	}
	*src = s;
	return 0;
}

// curl function
// write back in curlbuf
LASTFMWEB_DECL
size_t lastfmweb_write_callback(char *ptr, const size_t size, const size_t nmemb, curlbuf *cb)
{
	const size_t real = size * nmemb;
	curlbuf_append(cb, ptr, real);

	return real;
}

LASTFMWEB_DECL
char *lastfmweb_parse_url(lastfmweb_source *s, const char *_path, const char *_parameter, ...)
{
	va_list paras;
	char *realurl, *base_url,
		*url = NULL, *path = NULL,
		*hout = NULL, *parameter = NULL;
	size_t urlen;
	int bufstrlen;
	CURLU *h = NULL;

	base_url = s->base_url;
	// future: wrapper parseKVFormat with lastfm config map,
	// in order to automatically parse path
	path = parseKVFormat(_path,
			     MAKE_KV("username", (const char *)s->username), KV_END);

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

LASTFMWEB_DECL
int lastfmweb_prepare_curl(source *s, CURL *curl, struct curl_slist **h_ref, bool use_security, curlbuf *buf,
			   const char *method, const char *url, size_t header_count, ...)
{
	va_list hs;
	size_t i;
	if (use_security)
		source_perform_security(s);

	struct curl_slist *h = NULL;

#if 1
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

	// set url
	curl_easy_setopt(curl, CURLOPT_URL, url);
	// set method
	if (strcmp(method, "GET") == 0) {
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	} else if (strcmp(method, "POST") == 0) {
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
	}

	// set headers
	va_start(hs, header_count);

	for (i = 0; i < header_count; ++i) {
		const char *header = va_arg(hs, const char *);
		h = curl_slist_append(h, header);
	}

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);

	// set write callback
	curlbuf_init(buf, BUF_DEFAULT_CAP);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, lastfmweb_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

	*h_ref = h;
	return 0;
}

// json parser
#define JSON_ERR_HEAD "LASTFMWEB"

#define JSON_READ_ERR -1
#define JSON_PARSE_PLAYLIST_ERR -2
#define JSON_PARSE_TRACK_ERR -3
#define JSON_PARSE_URL_ERR -4
LASTFMWEB_DECL
artist *lastfmweb_json2artist(yyjson_val *val, bool strict, struct json_err *err)
{
	artist *ar;
	const char *name;

	name = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (name == NULL) {
		write_jsonerr(err, "[%s]: failed to parse field \"name\" "
				   "into string in artist.\n",
			      JSON_ERR_HEAD);
		mxrec_cleanup(cleanup, ar, 0);
	}

	// lastfm don't support alias
	ar = artist_new(name, 0, NULL);

cleanup:
	return ar;
}

LASTFMWEB_DECL
track *lastfmweb_json2track(yyjson_val *val, bool strict, struct json_err *err)
{
	track *tr;
	const char *title, *album;
	size_t i, ar_size;
	artist **artists, *a;
	yyjson_val *ars, *ar;

	// name
	title = yyjson_get_str(yyjson_obj_get(val, "name"));
	if (title == NULL) {
		write_jsonerr(err, "[%s]: failed to parse field \"name\" "
				   "into string in playlist item.\n",
			      JSON_ERR_HEAD);
		mxrec_cleanup(cleanup, tr, 0);
	}

	// album
	album = yyjson_get_str(yyjson_obj_get(val, "primary_album"));
	if (strict && album == NULL) {
		write_jsonerr(err, "[%s]: failed to get album information of %s\n.",
			      JSON_ERR_HEAD, title);
		mxrec_cleanup(cleanup, tr, 0);
	}

	// aritsts
	ars = yyjson_obj_get(val, "artists");
	if (!yyjson_is_arr(ars)) {
		write_jsonerr(err, "[%s]: artists field is not an array.\n", JSON_ERR_HEAD);
		mxrec_cleanup(cleanup, tr, 0);
	}

	ar_size = yyjson_arr_size(ars);
	artists = xmalloc(ar_size * sizeof(artists[0]));
	yyjson_arr_foreach(ars, i, ar_size, ar)
	{
		a = lastfmweb_json2artist(ar, strict, err);
		if (strict && a == NULL) {
			write_jsonerr(err, "[%s]: failed to get an artist information of %s\n",
				      JSON_ERR_HEAD, title);
			mxrec_cleanup(cleanup, tr, 0);
		}
		artists[i] = a;
	}

	if (artists[0] == NULL) {
		write_jsonerr(err, "[%s]: %s cann't get the artist information.", JSON_ERR_HEAD, title);
		mxrec_cleanup(cleanup, tr, 0);
	}

	// lastfm don't support alias
	tr = track_new(title, album, ar_size, artists, 0, NULL);
cleanup:
	return tr;
}

LASTFMWEB_DECL
int lastfmweb_json2playitem(yyjson_val *val, playitem *pi, bool strict, struct json_err *err)
{
	int ret;
	track *tr;

	yyjson_val *playlinks;
	yyjson_arr_iter iter;
	yyjson_val *playlink;
	const char *url;

	tr = lastfmweb_json2track(val, strict, err);
	if (tr == NULL)
		return JSON_PARSE_TRACK_ERR;
	pi->tr = tr;

	// add urls
	playlinks = yyjson_obj_get(val, "playlinks");
	if (!yyjson_is_arr(playlinks)) {
		if (strict) {
			write_jsonerr(err, "[%s]: playlinks field is not an array.\n", JSON_ERR_HEAD);
			write_jsonerr(err, "[%s]: ignore url error of %s\n", JSON_ERR_HEAD, tr->title);
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

LASTFMWEB_DECL
int lastfmweb_jsonbuf2playlist(curlbuf *buf, size_t wanted, playlist *p_ref, bool strict, struct json_err *jerr)
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
		write_jsonerr(jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n",
			      JSON_ERR_HEAD, err.msg, err.code, err.pos);
		mxrec_cleanup(cleanup, ret, JSON_READ_ERR);
	}

	root = yyjson_doc_get_root(doc);
	pls = yyjson_obj_get(root, "playlist");
	if (!yyjson_is_arr(pls)) {
		write_jsonerr(jerr, "[%s]: playlist field is not an array.\n", JSON_ERR_HEAD);
		mxrec_cleanup(cleanup, ret, JSON_PARSE_PLAYLIST_ERR);
	}

	plen = yyjson_arr_size(pls);

	plen = wanted <= plen ? wanted : plen;

	yyjson_arr_foreach(pls, i, plen, jpi)
	{
		int pi_ret;
		playitem_init(&pi);
		pi_ret = lastfmweb_json2playitem(jpi, &pi, strict, jerr);

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

LASTFMWEB_DECL
int lastfmweb_jsonbuf2playitem(curlbuf *buf, playitem *p, bool strict, struct json_err *jerr)
{
	int ret, pi_ret;
	yyjson_read_err err;
	yyjson_doc *doc;
	yyjson_val *root, *pls, *pi;

	doc = yyjson_read_opts(buf->buf, buf->len,
			       0, NULL, &err);

	if (doc == NULL) {
		write_jsonerr(jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n",
			      JSON_ERR_HEAD, err.msg, err.code, err.pos);
		mxrec_cleanup(cleanup, ret, JSON_READ_ERR);
	}

	root = yyjson_doc_get_root(doc);
	pls = yyjson_obj_get(root, "playlist");
	if (!yyjson_is_arr(pls)) {
		write_jsonerr(jerr, "[%s]: playlist field is not an array.\n", JSON_ERR_HEAD);
		mxrec_cleanup(cleanup, ret, JSON_PARSE_PLAYLIST_ERR);
	}

	pi = yyjson_arr_get_first(pls);
	if (pi == NULL) {
		write_jsonerr(jerr, "[%s]: playlist has no items.\n", JSON_ERR_HEAD);
		mxrec_cleanup(cleanup, ret, JSON_PARSE_PLAYLIST_ERR);
	}
	pi_ret = lastfmweb_json2playitem(pi, p, strict, jerr);
	if (strict && pi_ret < 0)
		mxrec_cleanup(cleanup, ret, pi_ret);
	else if (pi_ret == JSON_PARSE_TRACK_ERR)
		mxrec_cleanup(cleanup, ret, pi_ret);

	ret = 0;
cleanup:
	yyjson_doc_free(doc);
	return ret;
}

LASTFMWEB_DECL
int _lastfmweb_recomm_single_full(source *s, playitem *p, recomm_option opts)
{
	return -1;
}

LASTFMWEB_DECL
int _lastfmweb_recomm_single_simple(source *s, playitem *p, recomm_option opts)
{
	int ret;
	CURLcode code;
	curlbuf buf;
	struct json_err jerr;
	jerr.length = 0;
	long http_code = 0;
	assert(p);
	lastfmweb_source *ls = (lastfmweb_source *)s;
	CURL *curl = ls->curl;
	struct curl_slist *h = NULL;

	char *realurl = lastfmweb_parse_url(ls, ls->recomm_path, ls->recomm_parameter);
	if (realurl == NULL)
		mxrec_cleanup(cleanup, ret, -1);
	if (lastfmweb_prepare_curl(s, curl, &h, opts.use_security, &buf,
				   ls->recomm_method, realurl,
				   1, ls->recomm_accept) < 0)
		mxrec_cleanup(cleanup, ret, -1);
	code = curl_easy_perform(curl);
	if (code != CURLE_OK)
		mxrec_cleanup(cleanup, ret, -1);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code != 200)
		mxrec_cleanup(cleanup, ret, -1);

	if ((ret = lastfmweb_jsonbuf2playitem(&buf, p, opts.strict, &jerr)) < 0) {
		print_jsonerr(&jerr);
		mxrec_cleanup(cleanup, ret, ret);
	}

	ret = 0;
cleanup:
	xfree(realurl);
	curlbuf_free(&buf);
	if (h)
		curl_slist_free_all(h);
	curl_easy_reset(curl);
	return ret;
}

LASTFMWEB_DECL
void lastfmweb_source_destroy(void *sp)
{
	lastfmweb_source *s = (lastfmweb_source *)sp;
	lastfm_security_free(s->src.security);
	curl_easy_cleanup(s->curl);
	xfree(s->base_url);
	u8sfree(s->username);
	xfree(s->recomm_path);
	xfree(s->recomm_method);
	xfree(s->recomm_accept);
	xfree(s->recomm_parameter);
}

// interface implement
LASTFMWEB_DECL
int lastfmweb_recomm_single(source *s, playitem *p, recomm_option opts)
{
	log("Calling lastfm-web recomm_single\n");
	if (opts.level == RECOMM_SIMPLE)
		return _lastfmweb_recomm_single_simple(s, p, opts);
	else if (opts.level == RECOMM_FULL)
		return _lastfmweb_recomm_single_full(s, p, opts);
	else
		return -1;
}

/**
 * Fuck lastfm, the size of playlist captured from http request is not fixed.
 * So here if assigned `num` is smaller than playlist size it works normally,
 * else return playlist size.
 * Doing multi calls to meet `num` value is fucking silly.
 */
LASTFMWEB_DECL
int lastfmweb_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	log("Calling lastfm-web recomm_multi\n");
	int ret;
	CURLcode code;
	curlbuf buf;
	struct json_err jerr;
	jerr.length = 0;
	long http_code = 0;
	assert(p);
	if (opts.level == RECOMM_FULL) {
		panic("lastfm web source don't support full level recommendation");
	}
	assert(opts.level == RECOMM_SIMPLE);
	lastfmweb_source *ls = (lastfmweb_source *)s;
	CURL *curl = ls->curl;
	struct curl_slist *h = NULL;

	// set url
	char *realurl = lastfmweb_parse_url(ls, ls->recomm_path, ls->recomm_parameter);
	if (realurl == NULL)
		mxrec_cleanup(cleanup, ret, -1);

	if (lastfmweb_prepare_curl(s, curl, &h, opts.use_security, &buf,
				   ls->recomm_method, realurl,
				   1, ls->recomm_accept) < 0)
		mxrec_cleanup(cleanup, ret, -1);

	code = curl_easy_perform(curl);
	if (code != CURLE_OK)
		mxrec_cleanup(cleanup, ret, -1);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code != 200)
		mxrec_cleanup(cleanup, ret, -1);

	if ((ret = lastfmweb_jsonbuf2playlist(&buf, num, p,
					      opts.strict, &jerr)) < 0) {
		print_jsonerr(&jerr);
		mxrec_cleanup(cleanup, ret, -1);
	}

cleanup:
	xfree(realurl);
	curlbuf_free(&buf);
	if (h)
		curl_slist_free_all(h);
	curl_easy_reset(curl);
	return ret;
}

LASTFMWEB_DECL
void lastfmweb_security_handle(source *s)
{
	CURLcode code = CURLE_OK;
	lastfmweb_source *ls = (lastfmweb_source *)s;
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

#define check(expr, val)                                                       \
	do {                                                                   \
		if (!(expr)) {                                                 \
			(val) = false;                                         \
			error("lastfm-web source init failed on %s", (#expr)); \
		}                                                              \
	} while (0)

LASTFMWEB_DECL
bool lastfmweb_config_check(source *s)
{
	bool ret;
	lastfmweb_source *ls = (lastfmweb_source *)s;
	check(ls->username != NULL, ret);
	check(ls->base_url != NULL, ret);
	check(ls->recomm_path != NULL, ret);
	check(ls->recomm_method != NULL, ret);
	check(ls->recomm_accept != NULL, ret);
	return ret;
}
