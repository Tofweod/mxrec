#ifndef TOF_MXREC_SOURCE_LASTFM_WEB_H
#define TOF_MXREC_SOURCE_LASTFM_WEB_H

#include "comm-source.h"

#define LASTFMWEB_LEVEL_MASK 0

typedef struct lastfmweb_source lastfmweb_source;

int lastfmweb_source_new(source **src, config_t *cfg);

#endif
