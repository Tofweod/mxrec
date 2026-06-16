#include "al.h"
#include "assert.h"
#include "comm.h"

int random_absolute_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
				  struct random_absolute_params *params mxrec_unused)
{
	// TODO
	return 0;
}

int random_fair_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			      struct random_fair_params *params)
{
	if (unlikely(!params || !params->weights || params->count == 0)) {
		error("random_fair: invalid params");
		return -1;
	}

	// TODO

	return 0;
}
