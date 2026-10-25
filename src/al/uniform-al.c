#include "al-comm.h"
#include "assert.h"
#include "da.h"
#include "merge-state.h"
#include "xmalloc.h"

int uniform_merge_sources(merge_state *state, struct merge_params *params)
{
	(void)params;
	double *weights = state->source_count ? xmalloc(state->source_count * sizeof(*weights)) : NULL;
	for (size_t i = 0; i < state->source_count; i++)
		weights[i] = 1;
	int ret = merge_fill_quotas(state, weights);
	xfree(weights);
	return ret;
}
