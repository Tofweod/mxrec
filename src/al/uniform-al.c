#include "al-comm.h"
#include "assert.h"
#include "comm.h"

int uniform_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			  struct merge_params *params)
{
	struct uniform_params *p = (struct uniform_params *)params;
	size_t i, taken, base, rem, quota, got;

	if (unlikely(source_size == 0 || wanted == 0))
		return 0;
	if (unlikely(!p || !p->sample_source)) {
		error("uniform: sample function is NULL");
		return -1;
	}

	base = wanted / source_size;
	rem = wanted % source_size;
	taken = 0;

	for (i = 0; i < source_size; i++) {
		quota = base + (i < rem ? 1 : 0);
		if (quota == 0)
			continue;
		got = p->sample_source(result, quota, sources[i], 0, p->sample_data);
		taken += got;
	}

	return taken;
}
