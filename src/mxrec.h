#ifndef TOF_MXREC_H
#define TOF_MXREC_H

#include "al.h"
#include "config.h"
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
	MXREC_SOURCE(lastfmapi)                                                                                        \
	MXREC_SOURCE(lastfmweb)                                                                                        \
	MXREC_SOURCE(ncm)

enum mxrec_source_id {
#define MXREC_SOURCE(name) SOURCE_##name,
	MXREC_SOURCE_LIST
#undef MXREC_SOURCE
		MXREC_SOURCE_COUNT
};

extern source **sources;
extern int source_count;

void sources_build(config_t *cfg, const char **names, int count);

#endif
