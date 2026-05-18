#ifndef TOF_MXREC_PLAYLIST_H
#define TOF_MXREC_PLAYLIST_H

#include "dump.h"
typedef struct track track;

typedef struct playitem {
	dumpType *dt;
	track *tr;
	char *urls[];
} playitem;
typedef playitem *playlist;

void *playitem_new(track *tr);
int playitem_addurl(const char *url);
void playitem_free(playitem *pi);

void dumpPlaylist(playlist *pl);
void playlist_free(playlist *pl);

#endif
