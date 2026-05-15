#include "lastfm.h"
#include "assert.h"
#include "config.h"
#include "curl-impersonate.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/comm-source.h"
#include "utils/str.h"

typedef struct lastfm_security {
	char *profile;
} lastfm_security;

static int _securityinit(lastfm_security *security, config_t *cfg)
{
	security->profile = xstrdup(cfg->lastfm_security_profile);
	return 0;
}

static void _securityfree(lastfm_security *security)
{
	xfree(security->profile);
}

static lastfm_security __security;

#define FUNCTION_FIELD(retype, name, ...) static inline retype lastfm_##name(__VA_ARGS__);

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

static inline int lastfm_source_init(void *sp, config_t *cfg)
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

static inline int _lastfm_recomm_single_full(source *s, playentry *p, recomm_option opts)
{
	// TODO
}

static inline int _lastfm_recomm_single_simple(source *s, playentry *p, recomm_option opts)
{
}

static inline void lastfm_source_destroy(void *sp)
{
	printf("Calling lastfm destroy\n");
	// TODO
	lastfm_source *s = (lastfm_source *)sp;
	_securityfree(s->src.security);
	curl_easy_cleanup(s->curl);
}

// interface implement
static inline int lastfm_recomm_single(source *s, playentry *p, recomm_option opts)
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

static inline int lastfm_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	CURLcode code;
	if (opts.level == RECOMM_FULL) {
		panic("lastfm source don't support full level recommendation");
	}
	assert(opts.level == RECOMM_SIMPLE);
	lastfm_source *ls = (lastfm_source *)s;
	if (opts.use_security)
		perform_security(s);

	CURL *curl = ls->curl;

	code = curl_easy_perform(curl);
	if (code != CURLE_OK) {
		// TODO
	}
	curl_easy_reset(curl);
	return 0;
}

static inline void lastfm_security_handle(source *s)
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
