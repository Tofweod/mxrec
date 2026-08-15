#include "al-comm.h"
#include "assert.h"
#include "comm.h"
#include "da.h"

int priority_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			   struct merge_params *params)
{
	struct priority_params *p = (struct priority_params *)params;
	size_t i, taken, got, remaining;

	if (unlikely(source_size == 0 || wanted == 0))
		return 0;
	if (unlikely(!p || !p->sample_source)) {
		error("priority: sample function is NULL");
		return -1;
	}

	taken = 0;
	remaining = wanted;

	for (i = 0; i < source_size && remaining > 0; i++) {
		size_t src_len = da_len(sources[i]);
		size_t quota = remaining < src_len ? remaining : src_len;
		got = p->sample_source(result, quota, sources[i], 0, p->sample_data);
		taken += got;
		remaining -= got;
	}

	return taken;
}
