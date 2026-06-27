#ifndef TOF_MXREC_SOURCE_NCM_H
#define TOF_MXREC_SOURCE_NCM_H

#include "comm-source.h"

#define NCM_DEFAULT_HTTP_ADDRESS "127.0.0.1"
#define NCM_DEFAULT_SOCKET_ADDRESS "/tmp/mxrec.socket"

typedef struct ncm_source ncm_source;

int ncm_source_new(source **src, config_t *cfg);

void ncm_get_auth(source *s);

#endif
