#include "al-comm.h"
#include "assert.h"
#include "da.h"
#include "merge-state.h"

int round_robin_merge_sources(merge_state *state, struct merge_params *params)
{
	(void)params;
	while (da_len(*state->result) < state->wanted) {
		size_t added = 0;
		for (size_t i = 0; i < state->source_count && da_len(*state->result) < state->wanted; i++)
			added += merge_take_unique(state, i, 1);
		if (!added)
			break;
	}
	return (int)da_len(*state->result);
}
