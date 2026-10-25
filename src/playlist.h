#ifndef TOF_MXREC_PLAYLIST_H
#define TOF_MXREC_PLAYLIST_H

#include "dump.h"
#include <stdbool.h>
typedef struct track track;

#define MAX_URL_SIZE 512

typedef struct playitem {
	dumpHandle *dh;
	track *tr;
	unsigned int url_size;
	unsigned int url_alloc;
	char **urls;
} playitem;

// dynamic array using da.h
typedef playitem *playlist;

void playitem_init(playitem *pi);
int playitem_addurl(playitem *pi, const char *url);
/**
 * playitem will only free it's fields,
 * while playlist, track and artist will free themselves.
 */
void playitem_free(playitem *pi);

void dumpPlaylist(const playlist pl);
void playlist_free(playlist pl);

/* Stable deduplication of an owning source playlist. Removed items are freed.
 * On strict validation failure the list is unchanged. Returns its new length. */
int playlist_unique_owned(playlist *pl, size_t limit, bool strict);

#endif
