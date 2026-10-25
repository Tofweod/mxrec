#include "al-comm.h"
#include "assert.h"
#include "da.h"
#include "merge-state.h"
#include "random.h"
#include "xmalloc.h"

int reservoir_merge_sources(merge_state *state, struct merge_params *params)
{
	(void)params;
	track_key_set seen = {0};
	size_t visited = 0;
	int ret = -1;
	if (!state->wanted)
		return 0;
	for (size_t i = 0; i < state->source_count; i++) {
		merge_source_state *source = &state->sources[i];
		for (size_t j = 0; j < source->len; j++) {
			merge_candidate *candidate = &source->items[j];
			/* Remember every unique song, including displaced reservoir entries. */
			if (!track_key_set_add(&seen, &candidate->key))
				continue;
			visited++;
			if (da_len(*state->result) < state->wanted) {
				da_append(*state->result, *candidate->item);
			} else {
				double value;
				if (uniform01(&value) < 0)
					goto cleanup;
				size_t index = (size_t)(value * visited);
				if (index < state->wanted)
					(*state->result)[index] = *candidate->item;
			}
		}
	}
	ret = (int)da_len(*state->result);
cleanup:
	track_key_set_free(&seen);
	return ret;
}
