#ifndef TOF_MXREC_TRACK_H
#define TOF_MXREC_TRACK_H

#include "dump.h"
#include "u8string.h"

typedef struct artist artist;

typedef struct track {
	dumpType *dt;
	u8s title;
	artist **artists;
	unsigned int ar_size;
	u8s album;
	unsigned int alia_size;
	u8s alias[];
} track;

void *track_new(void);
void track_free(track *tr);

#endif
