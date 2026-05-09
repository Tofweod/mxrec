#ifndef TOF_MXREC_SOURCE_LASTFM_H
#define TOF_MXREC_SOURCE_LASTFM_H

#include "comm-source.h"

#define LASTFM_LEVEL_MASK 0

typedef struct lastfm_source lastfm_source;

int lastfm_source_new(source **src, config_t *cfg);

#endif
