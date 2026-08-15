#include "al-comm.h"
#include "comm.h"
#include "da.h"
#include "random.h"
#include "xmalloc.h"
#include <string.h>

int reservoir_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			    struct merge_params *params)
{
	(void)params;
	double *rands;
	size_t i, j, k, total, cnt;

	if (unlikely(source_size == 0 || wanted == 0))
		return 0;
	if (unlikely(!result))
		return -1;

	total = 0;
	for (i = 0; i < source_size; i++) {
		if (!sources[i])
			continue;
		total += da_len(sources[i]);
	}

	if (total <= wanted) {
		for (i = 0; i < source_size; i++) {
			size_t src_len;
			if (!sources[i])
				continue;
			src_len = da_len(sources[i]);
			da_append_arr(*result, sources[i], src_len);
		}
		return (int)total;
	}

	rands = xmalloc(total * sizeof(double));
	if (unlikely(uniform01Array(rands, total))) {
		xfree(rands);
		return -1;
	}

	cnt = 0;
	for (i = 0; i < source_size; i++) {
		if (!sources[i])
			continue;
		size_t src_len = da_len(sources[i]);
		for (j = 0; j < src_len; j++) {
			if (cnt < wanted) {
				da_append(*result, sources[i][j]);
			} else {
				k = (size_t)(rands[cnt] * (cnt + 1));
				if (k < wanted)
					(*result)[k] = sources[i][j];
			}
			cnt++;
		}
	}

	xfree(rands);
	return (int)wanted;
}
