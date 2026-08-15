#include "al-comm.h"
#include "assert.h"
#include "comm.h"
#include "da.h"

int weighted_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			   struct merge_params *params)
{
	struct weighted_params *p = (struct weighted_params *)params;
	size_t i, taken, got;
	double total_weight;

	if (unlikely(source_size == 0 || wanted == 0))
		return 0;
	if (unlikely(!p || !p->sample_source || !p->weights)) {
		error("weighted: sample function or weights is NULL");
		return -1;
	}
	if (unlikely(p->count != source_size)) {
		error("weighted: params->count (%zu) != source_size (%zu)", p->count, source_size);
		return -1;
	}

	total_weight = 0.0;
	for (i = 0; i < source_size; i++)
		total_weight += p->weights[i];

	if (unlikely(total_weight <= 0.0)) {
		error("weighted: total weight is zero");
		return -1;
	}

	taken = 0;
	for (i = 0; i < source_size; i++) {
		size_t src_len = da_len(sources[i]);
		size_t quota;
		if (src_len == 0)
			continue;
		quota = (size_t)(p->weights[i] / total_weight * wanted);
		if (quota > src_len)
			quota = src_len;
		got = p->sample_source(result, quota, sources[i], 0, p->sample_data);
		taken += got;
	}

	return taken;
}
