#include "dedup-helpers.h"
#include "utils/normalize.h"

static void normalized(const char *input, const char *expected)
{
	char *output = NULL;
	size_t len;
	CHECK(normalize_utf8_key(input, strlen(input), &output, &len) == 0);
	CHECK(len == strlen(expected) && memcmp(output, expected, len) == 0);
	xfree(output);
}

int main(void)
{
	normalized("  Ｓｏｎｇ\tＡ \n", "song a");
	normalized("Straße", "strasse");
	normalized("É", "é");
	normalized("E\xcc\x81", "é");
	normalized("\xc2\xa0 A\xe2\x80\x83\tB\xe3\x80\x80", "a b");
	normalized("\t\r\n\xc2\x85\xe2\x80\xa8", "");
	normalized("A\xe2\x80\x8b"
		   "B",
		   "a\xe2\x80\x8b"
		   "b");
	char *invalid = NULL;
	size_t len;
	CHECK(normalize_utf8_key("\xff", 1, &invalid, &len) < 0 && invalid == NULL);

	const char *a[] = {"Artist B", " Artist A ", "ARTIST A"};
	const char *b[] = {"artist a", "artist b"};
	playitem one = test_item(" ＳＯＮＧ ", "Album 1", a, 3);
	playitem two = test_item("song", "Album 2", b, 2);
	const char *aliases[] = {"A different alias"};
	artist_free(two.tr->artists[0]);
	two.tr->artists[0] = artist_new("artist a", 1, aliases);
	track_key k1 = {0}, k2 = {0};
	CHECK(track_key_build(one.tr, &k1) == 0);
	CHECK(track_key_build(two.tr, &k2) == 0 && track_key_equal(&k1, &k2));
	CHECK(strcmp((const char *)one.tr->title, " SONG ") == 0);
	track_key_free(&k2);
	u8sfree(two.tr->title);
	two.tr->title = u8snew("Song (Live)");
	CHECK(track_key_build(two.tr, &k2) == 0 && !track_key_equal(&k1, &k2));

	/* Force a hash collision: both distinct complete keys must survive. */
	k1.hash = k2.hash = 42;
	track_key_set set = {0};
	CHECK(track_key_set_add(&set, &k1) == 1);
	CHECK(track_key_set_add(&set, &k2) == 1);
	CHECK(track_key_set_add(&set, &k1) == 0);
	CHECK(track_key_set_contains(&set, &k2));
	track_key_free(&k1);
	track_key_free(&k2);
	track_key_set_free(&set);
	playitem_free(&one);
	playitem_free(&two);

	const char *left[] = {"bc"}, *right[] = {"c"};
	one = test_item("a", NULL, left, 1);
	two = test_item("ab", NULL, right, 1);
	CHECK(track_key_build(one.tr, &k1) == 0 && track_key_build(two.tr, &k2) == 0);
	CHECK(!track_key_equal(&k1, &k2));
	track_key_free(&k1);
	track_key_free(&k2);
	playitem_free(&one);
	playitem_free(&two);

	/* Failure after allocating an earlier artist must release partial keys. */
	const char *bad_artists[] = {"Valid Artist", " \t "};
	one = test_item("Title", NULL, bad_artists, 2);
	CHECK(track_key_build(one.tr, &k1) < 0 && k1.data == NULL && k1.len == 0);
	CHECK(track_key_build(NULL, &k1) < 0 && k1.data == NULL);
	artist_free(one.tr->artists[1]);
	one.tr->artists[1] = NULL;
	CHECK(track_key_build(one.tr, &k1) < 0 && k1.data == NULL);
	playitem_free(&one);
	puts("track-key-test passed");
	return 0;
}
