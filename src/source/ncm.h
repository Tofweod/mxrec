#ifndef TOF_MXREC_SOURCE_NCM_H
#define TOF_MXREC_SOURCE_NCM_H

#include "comm-source.h"

typedef struct ncm_source ncm_source;

int ncm_source_new(source **src, config_t * cfg);

#endif
