#include "sample.h"
#include "da.h"
#include "random.h"
#include "xmalloc.h"
#include <string.h>

size_t sample_head(playlist *dst, size_t n, playlist src, size_t cursor, void *params)
{
	size_t src_len, take;

	(void)params;
	src_len = da_len(src);
	if (cursor >= src_len)
		return 0;
	take = cursor + n <= src_len ? n : src_len - cursor;
	da_append_arr(*dst, src + cursor, take);
	return take;
}

size_t sample_random(playlist *dst, size_t n, playlist src, size_t cursor, void *params)
{
	double *rands;
	size_t src_len, take, i, k;

	(void)cursor;
	(void)params;
	src_len = da_len(src);
	take = n < src_len ? n : src_len;
	if (take == 0)
		return 0;

	rands = xmalloc(take * sizeof(double));
	if (uniform01Array(rands, take)) {
		xfree(rands);
		return 0;
	}

	for (i = 0; i < take; i++) {
		k = i + (size_t)(rands[i] * (src_len - i));
		da_append(*dst, src[k]);
		if (k != i) {
			playitem tmp = src[i];
			src[i] = src[k];
			src[k] = tmp;
		}
	}

	xfree(rands);
	return take;
}
