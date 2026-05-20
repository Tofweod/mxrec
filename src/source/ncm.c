#include "ncm.h"
#include "assert.h"
#include "config.h"
#include "playlist.h"
#include "source.h"
#include "source/comm-source.h"
#include "xmalloc.h"

typedef struct ncm_security {

} ncm_security;

static ncm_security __security;

#define FUNCTION_FIELD(retype, name, ...) static inline retype ncm_##name(__VA_ARGS__);

FUNCTION_FIELD_LIST

#undef FUNCTION_FIELD

static source __ncm_source = {
	.destroy = ncm_source_destroy,
	.rsp = ncm_recomm_single,
	.rmp = ncm_recomm_multi,
	.sh = ncm_security_handle,
	.security = &__security,
};

struct ncm_source {
	source src;
	char *email;
	char *cookie;
};

static int ncm_source_init(void *sp, config_t *cfg)
{
	ncm_source *s = (ncm_source *)sp;
	s->src = __ncm_source;

	s->cookie = xstrdup(cfg->ncm_cookie);

	return 0;
}

int ncm_source_new(source **src, config_t *cfg)
{
	assert(cfg);
	*src = NULL;
	void *s = xmalloc(sizeof(struct ncm_source));
	if (ncm_source_init(s, cfg) < 0) {
		xfree(s);
		return -1;
	}
	*src = s;
	return 0;
}

static inline void ncm_source_destroy(void *sp)
{
	// TODO
	ncm_source *s = (ncm_source *)sp;
	xfree(s->email);
	xfree(s->cookie);
}

static inline int ncm_recomm_single(source *s, playitem *p, recomm_option opts)
{
	// TODO
}

static inline int ncm_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	// TODO
}

static inline void ncm_security_handle(source *s)
{
}
