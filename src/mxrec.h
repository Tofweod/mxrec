#ifndef TOF_MXREC_H
#define TOF_MXREC_H

#include "al.h"
#include "source.h"

#define PROG_NAME "mxrec"

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

#define MXREC_SOURCE_LIST                                                                                              \
	MXREC_SOURCE(lastfm)                                                                                           \
	MXREC_SOURCE(ncm)

extern source *sources;

void sources_init();

#endif
