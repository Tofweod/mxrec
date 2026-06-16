#ifndef TOF_MXREC_AL_H
#define TOF_MXREC_AL_H

#include "playlist.h"

/**
 * MERGE_AL(prefix, order, params)
 * prefix: name of algorithm
 * order: less than 64, responding to flag bit of merge_sources
 * params: struct fields of specifical algorithm parameters
 */
#define MERGE_AL_LIST                                                                                                  \
	MERGE_AL(dummy, 0, )                                                                                           \
	MERGE_AL(random_fair, 1, double *weights; size_t count;)                                                       \
	MERGE_AL(random_absolute, 2, )

#define MERGE_AL(prefix, flag, ...) MERGE_FLAG_##prefix = 1UL << (flag),
enum merge_flag { MERGE_AL_LIST };
#undef MERGE_AL

#define MERGE_AL(prefix, flag, ...)                                                                                    \
	struct prefix##_params {                                                                                       \
		__VA_ARGS__                                                                                            \
	};
MERGE_AL_LIST
#undef MERGE_AL

#define MERGE_AL(prefix, flag, ...)                                                                                    \
	int prefix##_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,             \
				   struct prefix##_params *params);
MERGE_AL_LIST
#undef MERGE_AL

// data is void *; dispatcher casts it to the correct struct pointer internally.
int merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size, enum merge_flag flag,
		  void *data);

#endif // !TOF_MXREC_AL_H
