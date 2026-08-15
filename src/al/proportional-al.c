#include "al-comm.h"
#include "assert.h"
#include "comm.h"
#include "da.h"

int proportional_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			       struct merge_params *params)
{
	struct proportional_params *p = (struct proportional_params *)params;
	size_t i, taken, got, total;

	if (unlikely(source_size == 0 || wanted == 0))
		return 0;
	if (unlikely(!p || !p->sample_source)) {
		error("proportional: sample function is NULL");
		return -1;
	}

	total = 0;
	for (i = 0; i < source_size; i++)
		total += da_len(sources[i]);

	taken = 0;
	for (i = 0; i < source_size; i++) {
		size_t src_len = da_len(sources[i]);
		size_t quota;
		if (src_len == 0 || total == 0)
			continue;
		quota = (size_t)((double)src_len / total * wanted);
		if (quota > src_len)
			quota = src_len;
		got = p->sample_source(result, quota, sources[i], 0, p->sample_data);
		taken += got;
	}

	return taken;
}
