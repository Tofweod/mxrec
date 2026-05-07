#include "ncm.h"
#include "playlist.h"
#include "source.h"
#include "source/comm-source.h"

#define FUNCTION_FIELD(retype, name, ...) static inline retype ncm_##name(__VA_ARGS__);

FUNCTION_FIELD_LIST

#undef FUNCTION_FIELD

static source __ncm_source = {
	.destroy = ncm_source_destroy,
	.rsp = ncm_recomm_single,
	.rmp = ncm_recomm_multi,
};

struct ncm_source {
	source s;
};

static inline void ncm_source_destroy(void *sp)
{
	// TODO
}

static inline int ncm_recomm_single(source *s, playentry *p, recomm_option opts)
{
	// TODO
}

static inline int ncm_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	// TODO
}
