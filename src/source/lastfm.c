#include "lastfm.h"
#include "source.h"
#include <curl/curl.h>

typedef struct lastfm_security {

} lastfm_security;

// function declaition
#define FUNCTION_FIELD_LIST                                                     \
	FUNCTION_FIELD(void, source_destroy, void *)                            \
	FUNCTION_FIELD(int, recomm_single, source *, playlist *, recomm_option) \
	FUNCTION_FIELD(int, recomm_multi, source *, size_t, playlist *, recomm_option)

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

static inline void lastfm_source_destroy(void *sp)
{
	printf("Calling lastfm destroy\n");
	// TODO
}

static inline int lastfm_recomm_single(source *s, playlist *p, recomm_option opts)
{
	printf("Calling lastfm recomm_single\n");
	// TODO
}

static inline int lastfm_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	// TODO
}
