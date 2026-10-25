#include "dedup-helpers.h"
#include "source.h"
#include <limits.h>

static int calls, mode;

static int fake_recomm(source *s, size_t num, playlist *pl, recomm_option opts)
{
	(void)s;
	(void)num;
	(void)opts;
	calls++;
	const char *titles[] = {"X", "x", mode == 1 ? " " : "Y", "Z"};
	*pl = test_playlist(titles, 4);
	return mode == 2 ? -7 : 4;
}

int main(void)
{
	source s = {.rmp = fake_recomm};
	playlist pl = NULL;
	CHECK(recomm_multi(&s, 4, &pl, .strict = true) == 3);
	test_unique(pl);
	playlist_free(pl);
	CHECK(recomm_multi(&s, 2, &pl, .strict = true) == 2);
	playlist_free(pl);
	int before = calls;
	CHECK(recomm_multi(&s, 0, &pl, .strict = true) == 0 && pl == NULL && calls == before);
	CHECK(recomm_multi(&s, (size_t)INT_MAX + 1, &pl) < 0 && calls == before);
	mode = 1;
	CHECK(recomm_multi(&s, 4, &pl, .strict = true) < 0 && pl == NULL);
	CHECK(recomm_multi(&s, 4, &pl, .strict = false) == 2);
	playlist_free(pl);
	mode = 2;
	CHECK(recomm_multi(&s, 4, &pl) == -7 && pl == NULL);
	puts("source-unique-test passed");
	return 0;
}
