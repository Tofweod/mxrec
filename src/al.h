#ifndef TOF_MXREC_AL_H
#define TOF_MXREC_AL_H

#include "playlist.h"
#include "sample.h"
#include <stdint.h>

struct merge_params;
struct merge_state;
typedef int (*merge_source_fp)(struct merge_state *, struct merge_params *);
#define MERGE_AL_LIST                                                                                                  \
	MERGE_AL(dummy, )                                                                                              \
	MERGE_AL(uniform, )                                                                                            \
	MERGE_AL(proportional, )                                                                                       \
	MERGE_AL(weighted, double *weights; size_t count;)                                                             \
	MERGE_AL(priority, )                                                                                           \
	MERGE_AL(round_robin, )                                                                                        \
	MERGE_AL(reservoir, )

struct merge_params {
	merge_source_fp merge_source;
	enum sample_method sample;
};

/* result must be initialized and empty; it borrows source items.
 * Returns its length, or -1 with an empty result on error. */

int merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size, struct merge_params *params);

#endif
