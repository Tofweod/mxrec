#ifndef TOF_MXREC_PLAYLIST_VIEWER_H
#define TOF_MXREC_PLAYLIST_VIEWER_H
#include "playlist.h"
#include "sample.h"
#include "track-key.h"
#include <stddef.h>

typedef struct playlist_segment {
	playlist items;
	struct playlist_segment *next;
} playlist_segment;
typedef struct playlist_viewer {
	playlist_segment *head, *tail;
	const playitem **unique;
	size_t len, cap;
	track_key_set seen;
} playlist_viewer;
typedef struct source_result {
	playlist_viewer viewer;
} source_result;
typedef struct playlist_iterator {
	const playlist_viewer *viewer;
	enum sample_method method;
	size_t indexed, cursor, pending_len, pending_cap;
	const playitem **pending;
} playlist_iterator;
/* Takes ownership of batch, including on error. */
int source_result_append(source_result *result, playlist batch, bool strict);
void source_result_free(source_result *result);
void playlist_iterator_init(playlist_iterator *it, const playlist_viewer *viewer, enum sample_method method);
/* 1 = item, 0 = exhausted, -1 = error. */
int playlist_iterator_next(playlist_iterator *it, const playitem **item);
void playlist_iterator_free(playlist_iterator *it);
#endif
