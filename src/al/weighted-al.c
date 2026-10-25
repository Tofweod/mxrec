#include "al-comm.h"
#include "assert.h"
#include "da.h"
#include "merge-state.h"

int weighted_merge_sources(merge_state *state, struct merge_params *params)
{
	struct weighted_params *p = (struct weighted_params *)params;
	if (p->count != state->source_count || (!p->weights && p->count))
		return -1;
	return merge_fill_quotas(state, p->weights);
}
