#ifndef TOF_MXREC_AL_H
#define TOF_MXREC_AL_H

#include "playlist.h"

struct merge_params;
typedef int (*merge_fn)(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			struct merge_params *params);

struct merge_params {
	int (*merge_source)(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			    struct merge_params *params);
	size_t (*sample_source)(playlist *dst, size_t n, playlist src, size_t cursor, void *params);
	void *sample_data;
};

int merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size, struct merge_params *params);

#endif
