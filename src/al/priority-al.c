#include "al-comm.h"
#include "assert.h"
#include "da.h"
#include "merge-state.h"

int priority_merge_sources(merge_state *state, struct merge_params *params)
{
	(void)params;
	for (size_t i = 0; i < state->source_count && da_len(*state->result) < state->wanted; i++)
		merge_take_unique(state, i, state->wanted - da_len(*state->result));
	return (int)da_len(*state->result);
}
