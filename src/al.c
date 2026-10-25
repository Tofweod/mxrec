#include "al.h"
#include "al/merge-state.h"
#include "assert.h"
#include "comm.h"
#include "da.h"
#include <limits.h>

int merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size, struct merge_params *p)
{
	merge_state state;
	int ret;
	if (!p || !p->merge_source || !result || !*result || da_len(*result) || wanted > INT_MAX) {
		error("merge_sources: invalid parameters or nonempty result");
		return -1;
	}
	ret = merge_state_init(&state, result, wanted, sources, source_size, p->sample);
	if (ret == 0)
		ret = p->merge_source(&state, p);
	merge_state_free(&state);
	if (ret < 0)
		DAHDR(*result)->len = 0;
	return ret;
}
