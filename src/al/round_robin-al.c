#include "al-comm.h"
#include "assert.h"
#include "comm.h"
#include "da.h"

int round_robin_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			      struct merge_params *params)
{
	struct round_robin_params *p = (struct round_robin_params *)params;
	size_t taken, got, round;

	if (unlikely(source_size == 0 || wanted == 0))
		return 0;
	if (unlikely(!p || !p->sample_source)) {
		error("round_robin: sample function is NULL");
		return -1;
	}

	taken = 0;
	round = 0;

	while (taken < wanted) {
		size_t i, any = 0;
		for (i = 0; i < source_size && taken < wanted; i++) {
			if (round >= da_len(sources[i]))
				continue;
			got = p->sample_source(result, 1, sources[i], round, p->sample_data);
			taken += got;
			any += got;
		}
		if (any == 0)
			break;
		round++;
	}

	return taken;
}
