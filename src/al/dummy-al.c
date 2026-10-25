#include "al-comm.h"
#include "assert.h"
#include "da.h"
#include "merge-state.h"

int dummy_merge_sources(merge_state *state, struct merge_params *params)
{
	(void)state;
	(void)params;
	return -1;
}
