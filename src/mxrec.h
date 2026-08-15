#ifndef TOF_MXREC_H
#define TOF_MXREC_H

#include "al.h"		// IWYU pragma: keep
#include "al/al-comm.h" // IWYU pragma: keep
#include "config.h"
#include "dump.h"
#include "sample.h" // IWYU pragma: keep
#include "source.h"

#define PROG_NAME "mxrec"

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
extern size_t source_count;

void sources_build(config_t *cfg, const char **names, size_t count);

#define DUMP_TYPE_STR_SIZE 10UL
#define DUMP_TYPE_LIST DUMP_TYPE(json, JSON)

enum dumpType str2dumptype(const char *type_str);

#endif
