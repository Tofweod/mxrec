/* Exercise the real candidate selector offline; unused network code is discarded. */
#include "dedup-helpers.h"
#include "source/lastfm-api.c"

static size_t tracks_created, artists_created;

void *__real_track_new(const char *, const char *, unsigned, artist **, unsigned, const char *[]);
artist *__real_artist_new(const char *, unsigned, const char *[]);

void *__wrap_track_new(const char *title, const char *album, unsigned n, artist **artists, unsigned aliases,
		       const char *names[])
{
	tracks_created++;
	return __real_track_new(title, album, n, artists, aliases, names);
}

artist *__wrap_artist_new(const char *name, unsigned n, const char *aliases[])
{
	artists_created++;
	return __real_artist_new(name, n, aliases);
}

static playlist select_songs(la_track **candidates, size_t n, size_t wanted, bool strict, int expected, size_t created)
{
	playlist out = NULL;
	tracks_created = artists_created = 0;
	CHECK(lastfmapi_select_unique(candidates, n, wanted, &out, strict) == expected);
	CHECK(tracks_created == created && artists_created == created);
	if (expected < 0) {
		CHECK(out == NULL);
	} else {
		CHECK(da_len(out) == (size_t)expected);
		test_unique(out);
		CHECK(playlist_unique_owned(&out, wanted, strict) == expected);
	}
	return out;
}

int main(void)
{
	/* A large duplicate prefix must not hide later distinct songs. */
	la_track *candidates[196];
	for (size_t i = 0; i < 192; i++)
		candidates[i] = la_track_new(i % 2 ? "ＳＯＮＧ" : " Song ", i % 2 ? "ARTIST" : " artist ", NULL, NULL);
	candidates[192] = la_track_new("Second", "Artist", NULL, NULL);
	candidates[193] = la_track_new("second", "Artist", NULL, NULL);
	candidates[194] = la_track_new("Third", "Artist", NULL, NULL);
	/* Once target is reached, even an invalid tail must remain unexamined. */
	candidates[195] = la_track_new(" ", "Artist", NULL, NULL);
	playlist out = select_songs(candidates, 196, 3, true, 3, 3);
	CHECK(strcmp((const char *)out[0].tr->title, " Song ") == 0);
	CHECK(strcmp((const char *)out[1].tr->title, "Second") == 0);
	CHECK(strcmp((const char *)out[2].tr->title, "Third") == 0);
	playlist_free(out);
	out = select_songs(candidates, 195, 10, true, 3, 3);
	playlist_free(out);
	out = select_songs(candidates, 196, 10, false, 3, 3);
	playlist_free(out);
	select_songs(candidates, 196, 10, true, -1, 3);
	out = select_songs(candidates, 196, 0, true, 0, 0);
	playlist_free(out);
	out = select_songs(NULL, 0, 3, true, 0, 0);
	playlist_free(out);
	for (size_t i = 0; i < 196; i++)
		la_track_free(candidates[i]);

	la_track *bad[] = {la_track_new("Valid", "Artist", NULL, NULL),
			   la_track_new("Missing artist", NULL, NULL, NULL),
			   la_track_new("Other", "Artist", NULL, NULL)};
	select_songs(bad, 3, 3, true, -1, 1);
	out = select_songs(bad, 3, 3, false, 2, 2);
	playlist_free(out);
	for (size_t i = 0; i < 3; i++)
		la_track_free(bad[i]);
	puts("lastfm-api-unique-test passed");
	return 0;
}
