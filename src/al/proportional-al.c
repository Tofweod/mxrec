#include "al-comm.h"
#include "assert.h"
#include "da.h"
#include "merge-state.h"
#include "xmalloc.h"

int proportional_merge_sources(merge_state *state, struct merge_params *params)
{
	(void)params;
	double *weights = state->source_count ? xmalloc(state->source_count * sizeof(*weights)) : NULL;
	bool any = false;
	for (size_t i = 0; i < state->source_count; i++) {
		weights[i] = state->sources[i].len;
		any |= state->sources[i].len > 0;
	}
	int ret = any ? merge_fill_quotas(state, weights) : 0;
	xfree(weights);
	return ret;
}
