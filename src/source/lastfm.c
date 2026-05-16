#include "lastfm.h"
#include "assert.h"
#include "config.h"
#include "curl-impersonate.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/comm-source.h"
#include "utils/string.h"
#include "xmalloc.h"
#include <stddef.h>

#define LASTFM_DECL static

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
	if (s == NULL)
		return -1;
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
	char *result, *url, *path = NULL, *parameter = NULL;
	size_t urlen;
	int bufstrlen;

	config_t *cfg = s->cfg;
	url = cfg->lastfm_base_url;
	// future: wrapper parseKVFormat with lastfm config map,
	// in order to automatically parse path
	path = parseKVFormat(_path,
			     MAKE_KV("username", (const char *)cfg->lastfm_username), KV_END);

	if (path == NULL) {
		result = NULL;
		goto cleanup;
	}

	va_start(paras, _parameter);
	if (_parameter)
		parameter = parsevFormat(_parameter, paras);
	va_end(paras);

	if (_parameter && parameter == NULL) {
		result = NULL;
		goto cleanup;
	}

	urlen = strlen(url);
	result = xmalloc(urlen);
	if (result == NULL) {
		result = NULL;
		goto cleanup;
	}

	while (1) {
		if (_parameter) {
			bufstrlen = snprintf(result, urlen, "%s%s?%s", url, path, parameter);
		} else {
			bufstrlen = snprintf(result, urlen, "%s%s", url, path);
		}
		if (bufstrlen < 0) {
			result = NULL;
			goto cleanup;
		}
		if ((size_t)bufstrlen >= urlen) {
			urlen = ((size_t)bufstrlen) + 1;
			result = xrealloc(result, urlen);
			if (result == NULL)
				goto cleanup;
			continue;
		}
		break;
	}

cleanup:
	xfree(path);
	xfree(parameter);
	return result;
}

LASTFM_DECL
int _lastfm_recomm_single_full(source *s, playentry *p, recomm_option opts)
{
	// TODO
}

LASTFM_DECL
int _lastfm_recomm_single_simple(source *s, playentry *p, recomm_option opts)
{
	// TODO
}

LASTFM_DECL
void lastfm_source_destroy(void *sp)
{
	printf("Calling lastfm destroy\n");
	// TODO
	lastfm_source *s = (lastfm_source *)sp;
	_securityfree(s->src.security);
	curl_easy_cleanup(s->curl);
}

// interface implement
LASTFM_DECL
int lastfm_recomm_single(source *s, playentry *p, recomm_option opts)
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

LASTFM_DECL
int lastfm_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	int ret;
	CURLcode code;
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

#if 1
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

	// set url
	char *realurl = lastfm_parse_url(ls,
					 cfg->lastfm_mrc_path,
					 cfg->lastfm_mrc_parameter, num);
	if (realurl == NULL) {
		ret = -1;
		goto cleanup;
	}
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

	code = curl_easy_perform(curl);
	if (code != CURLE_OK) {
		// TODO errors like 404
		ret = -1;
		goto cleanup;
	}
	ret = 0;

cleanup:
	xfree(realurl);
	if (h)
		curl_slist_free_all(h);
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
