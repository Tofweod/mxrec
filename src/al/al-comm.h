#ifndef TOF_MXREC_AL_COMM_H
#define TOF_MXREC_AL_COMM_H

#include "al.h"

#define MERGE_AL(prefix, ...)                                                                                          \
	struct prefix##_params {                                                                                       \
		struct merge_params base;                                                                              \
		__VA_ARGS__                                                                                            \
	};
MERGE_AL_LIST
#undef MERGE_AL

#define MERGE_AL(prefix, ...) int prefix##_merge_sources(struct merge_state *state, struct merge_params *params);
MERGE_AL_LIST
#undef MERGE_AL

#endif
