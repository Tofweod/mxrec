#ifndef TOF_MXREC_TRACK_KEY_H
#define TOF_MXREC_TRACK_KEY_H

#include "track.h"
#include <stdbool.h>
#include <stddef.h>

/* Independent of track; owns normalized, length-prefixed title/artist bytes. */
typedef struct track_key {
	unsigned char *data;
	size_t len;
	unsigned hash;
} track_key;

int track_key_build(const track *tr, track_key *out);
bool track_key_equal(const track_key *a, const track_key *b);
void track_key_free(track_key *key);

struct track_key_entry;
typedef struct track_key_set {
	struct track_key_entry *head;
} track_key_set;

/* The set copies keys; callers retain ownership of their originals.
 * add returns 1 for a new key, 0 for an existing key. */
bool track_key_set_contains(const track_key_set *set, const track_key *key);
int track_key_set_add(track_key_set *set, const track_key *key);
void track_key_set_free(track_key_set *set);

#endif
