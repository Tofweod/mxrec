#include "al.h"
#include "assert.h"
#include "comm.h"

int merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size, struct merge_params *p)
{
	if (unlikely(!p || !p->merge_source)) {
		error("merge_sources: merge function is NULL");
		return -1;
	}

	return p->merge_source(result, wanted, sources, source_size, p);
}
