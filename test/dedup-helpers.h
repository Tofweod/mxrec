#ifndef MXREC_TEST_DEDUP_HELPERS_H
#define MXREC_TEST_DEDUP_HELPERS_H

#include "artist.h"
#include "assert.h"
#include "da.h"
#include "playlist.h"
#include "track-key.h"
#include "xmalloc.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                                                                    \
	do {                                                                                                           \
		if (!(expr)) {                                                                                         \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);                                     \
			exit(1);                                                                                       \
		}                                                                                                      \
	} while (0)

static inline playitem test_item(const char *title, const char *album, const char **names, unsigned n)
{
	playitem item;
	playitem_init(&item);
	artist **artists = xmalloc(n * sizeof(*artists));
	for (unsigned i = 0; i < n; i++)
		artists[i] = artist_new(names[i], 0, NULL);
	item.tr = track_new(title, album, n, artists, 0, NULL);
	return item;
}

static inline playlist test_playlist(const char **titles, size_t n)
{
	playlist result = NULL;
	const char *artists[] = {"Artist"};
	da_init(result, sizeof(*result));
	for (size_t i = 0; i < n; i++) {
		playitem item = test_item(titles[i], NULL, artists, 1);
		CHECK(playitem_addurl(&item, "https://example.invalid/song") == 0);
		da_append(result, item);
	}
	return result;
}

static inline void test_unique(playlist pl)
{
	track_key_set seen = {0};
	for (size_t i = 0; i < da_len(pl); i++) {
		track_key key = {0};
		CHECK(track_key_build(pl[i].tr, &key) == 0);
		CHECK(track_key_set_add(&seen, &key) == 1);
		track_key_free(&key);
	}
	track_key_set_free(&seen);
}

#endif
