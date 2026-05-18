#ifndef TOF_MXREC_ARTIST_H
#define TOF_MXREC_ARTIST_H

#include "dump.h"
#include "u8string.h"

typedef struct artist {
	dumpType *dt;
	u8s name;
	unsigned int alia_size;
	u8s alias[];
} artist;

void *artist_new(void);
void artist_free(artist *ar);
int aritst_add_alia(artist **ar_ref,const char* alia);

#endif
