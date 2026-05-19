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

/**
 * artist will take the control of field's lifetime
 */
artist *artist_new(const char *name, unsigned int alia_size, const char *alias[]);
void artist_free(artist *ar);

#endif
