#ifndef TOF_MXREC_AL_H
#define TOF_MXREC_AL_H

#include "playlist.h"
#include <stdint.h>

struct merge_params;
#define MERGE_AL_LIST                                                                                                  \
	MERGE_AL(dummy, )                                                                                              \
	MERGE_AL(uniform, )                                                                                            \
	MERGE_AL(proportional, )                                                                                       \
	MERGE_AL(weighted, double *weights; size_t count;)                                                             \
	MERGE_AL(priority, )                                                                                           \
	MERGE_AL(round_robin, )                                                                                        \
	MERGE_AL(reservoir, uint64_t seed;)

struct merge_params {
	int (*merge_source)(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			    struct merge_params *params);
	size_t (*sample_source)(playlist *dst, size_t n, playlist src, size_t cursor, void *params);
	void *sample_data;
};

int merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size, struct merge_params *params);

#endif
