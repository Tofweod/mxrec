#ifndef TOF_MXREC_AL_COMM_H
#define TOF_MXREC_AL_COMM_H

#include "al.h"
#include "playlist.h"
#include <stdint.h>

#define MERGE_AL_LIST                                                                                                  \
	MERGE_AL(dummy, )                                                                                              \
	MERGE_AL(uniform, )                                                                                            \
	MERGE_AL(proportional, )                                                                                       \
	MERGE_AL(weighted, double *weights; size_t count;)                                                             \
	MERGE_AL(priority, )                                                                                           \
	MERGE_AL(round_robin, )                                                                                        \
	MERGE_AL(reservoir, uint64_t seed;)

#define MERGE_AL(prefix, ...)                                                                                          \
	struct prefix##_params {                                                                                       \
		int (*merge_source)(playlist * result, size_t wanted, playlist *sources, size_t source_size,           \
				    struct merge_params *params);                                                      \
		size_t (*sample_source)(playlist * dst, size_t n, playlist src, size_t cursor, void *params);          \
		void *sample_data;                                                                                     \
		__VA_ARGS__                                                                                            \
	};
MERGE_AL_LIST
#undef MERGE_AL

#define MERGE_AL(prefix, ...)                                                                                          \
	int prefix##_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,             \
				   struct merge_params *params);
MERGE_AL_LIST
#undef MERGE_AL

#endif
