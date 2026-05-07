#include "lastfm.h"
#include "assert.h"
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
};

static inline int lastfm_source_init(void *sp)
{
	lastfm_source *s = (lastfm_source *)sp;
	s->src = __lastfm_source;

	// TODO another initialization works
	return 0;
}

int lastfm_source_new(source **src)
{
	*src = NULL;
	void *s = xmalloc(sizeof(struct lastfm_source));
	if (s == NULL)
		return -1;
	if (lastfm_source_init(s) < 0) {
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
	// TODO
}

static inline void lastfm_source_destroy(void *sp)
{
	printf("Calling lastfm destroy\n");
	// TODO
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
		// error report
		return -1;
	}
	assert(opts.level == RECOMM_SIMPLE);
	// TODO
}
