#ifndef TOF_MXREC_SAMPLE_H
#define TOF_MXREC_SAMPLE_H

#include "playlist.h"

struct sample_random_params {
	int _placeholder;
};

size_t sample_head(playlist *dst, size_t n, playlist src, size_t cursor, void *params);
size_t sample_random(playlist *dst, size_t n, playlist src, size_t cursor, void *params);

#endif
