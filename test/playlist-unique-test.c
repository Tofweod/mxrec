#include "dedup-helpers.h"

int main(void)
{
	const char *titles[] = {"X", "x", "Y", "Ｘ", "Z", "y", "Z"};
	playlist pl = test_playlist(titles, 7);
	track *x = pl[0].tr, *y = pl[2].tr, *z = pl[4].tr;
	CHECK(playlist_unique_owned(&pl, 10, true) == 3);
	CHECK(da_len(pl) == 3 && pl[0].tr == x && pl[1].tr == y && pl[2].tr == z);
	test_unique(pl);
	CHECK(playlist_unique_owned(&pl, 10, true) == 3);
	playlist_free(pl);

	pl = test_playlist(titles, 7);
	CHECK(playlist_unique_owned(&pl, 2, true) == 2);
	playlist_free(pl);
	pl = test_playlist(titles, 7);
	CHECK(playlist_unique_owned(&pl, 0, true) == 0 && da_len(pl) == 0);
	playlist_free(pl);

	const char *invalid[] = {"X", " ", "Y", "x"};
	pl = test_playlist(invalid, 4);
	x = pl[0].tr;
	CHECK(playlist_unique_owned(&pl, 4, true) < 0);
	CHECK(da_len(pl) == 4 && pl[0].tr == x);
	CHECK(playlist_unique_owned(&pl, 4, false) == 2);
	test_unique(pl);
	playlist_free(pl);
	pl = NULL;
	CHECK(playlist_unique_owned(&pl, 4, true) == 0);
	puts("playlist-unique-test passed");
	return 0;
}
