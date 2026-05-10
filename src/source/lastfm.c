#include "lastfm.h"
#include "assert.h"
#include "config.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/comm-source.h"
#include <curl/curl.h>

typedef struct lastfm_security {

} lastfm_security;

#define FUNCTION_FIELD(retype, name, ...) static inline retype lastfm_##name(__VA_ARGS__);

FUNCTION_FIELD_LIST

#undef FUNCTION_FIELD

static source __lastfm_source = {
	.destroy = lastfm_source_destroy,
	.rsp = lastfm_recomm_single,
	.rmp = lastfm_recomm_multi,
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

	s->curl = curl_easy_init();

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
	curl_easy_cleanup(s->curl);
}

static inline int lastfm_recomm_single(source *s, playentry *p, recomm_option opts)
{
	printf("Calling lastfm recomm_single\n");
	if (opts.level == RECOMM_SIMPLE)
		return _lastfm_recomm_single_simple(s, p, opts);
	else if (opts.level == RECOMM_FULL)
		return _lastfm_recomm_single_full(s, p, opts);
	else
		return -1;
}

static inline int lastfm_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	if (opts.level == RECOMM_FULL) {
		panic("lastfm source don't support full level recommendation");
	}
	assert(opts.level == RECOMM_SIMPLE);
	lastfm_source *ls = (lastfm_source *)s;
	CURL *curl = ls->curl;

	curl_easy_perform(curl);
	/* curl_easy_reset(curl); */
	return 0;
}
