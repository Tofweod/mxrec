#include "track-key.h"
#include "artist.h"
#include "uthash.h"
#include "utils/normalize.h"
#include "xmalloc.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

struct key_text {
	char *data;
	size_t len;
};

static int text_cmp(const void *pa, const void *pb)
{
	const struct key_text *a = pa, *b = pb;
	size_t n = a->len < b->len ? a->len : b->len;
	int cmp = memcmp(a->data, b->data, n);
	return cmp ? cmp : (a->len > b->len) - (a->len < b->len);
}

static void write_length(unsigned char **dst, uint64_t value)
{
	for (int shift = 56; shift >= 0; shift -= 8)
		*(*dst)++ = (unsigned char)(value >> shift);
}

int track_key_build_fields(track_key_text title_text, const track_key_text *artist_texts, size_t artist_count,
			   track_key *out)
{
	struct key_text title = {0}, *artists = NULL;
	size_t count = 0, total;
	unsigned char *dst;
	int ret = -1;
	if (!out)
		return -1;
	memset(out, 0, sizeof(*out));
	if (!title_text.data || !artist_count || !artist_texts || artist_count > SIZE_MAX / sizeof(*artists))
		return -1;
	if (normalize_utf8_key(title_text.data, title_text.len, &title.data, &title.len) < 0 || !title.len ||
	    title.len > UINT_MAX - 16)
		goto cleanup;
	artists = xcalloc(artist_count, sizeof(*artists));
	for (size_t i = 0; i < artist_count; i++) {
		if (normalize_utf8_key(artist_texts[i].data, artist_texts[i].len, &artists[i].data, &artists[i].len) <
			    0 ||
		    !artists[i].len)
			goto cleanup;
	}
	qsort(artists, artist_count, sizeof(*artists), text_cmp);
	total = 16 + title.len;
	for (size_t i = 0; i < artist_count; i++) {
		if (i && text_cmp(&artists[i - 1], &artists[i]) == 0)
			continue;
		if (total > UINT_MAX - 8 || artists[i].len > UINT_MAX - total - 8)
			goto cleanup;
		total += 8 + artists[i].len;
		count++;
	}
	out->data = xmalloc(total);
	out->len = total;
	dst = out->data;
	write_length(&dst, title.len);
	memcpy(dst, title.data, title.len);
	dst += title.len;
	write_length(&dst, count);
	for (size_t i = 0; i < artist_count; i++) {
		if (i && text_cmp(&artists[i - 1], &artists[i]) == 0)
			continue;
		write_length(&dst, artists[i].len);
		memcpy(dst, artists[i].data, artists[i].len);
		dst += artists[i].len;
	}
	HASH_VALUE(out->data, (unsigned)out->len, out->hash);
	ret = 0;
cleanup:
	if (artists) {
		for (size_t i = 0; i < artist_count; i++)
			xfree(artists[i].data);
	}
	xfree(artists);
	xfree(title.data);
	return ret;
}

int track_key_build(const track *tr, track_key *out)
{
	if (!out)
		return -1;
	memset(out, 0, sizeof(*out));
	if (!tr || !tr->title || !tr->ar_size || !tr->artists)
		return -1;
	track_key_text *artists = xcalloc(tr->ar_size, sizeof(*artists));
	for (unsigned i = 0; i < tr->ar_size; i++) {
		if (tr->artists[i] && tr->artists[i]->name)
			artists[i] =
				(track_key_text){(const char *)tr->artists[i]->name, u8sblen(tr->artists[i]->name)};
	}
	int ret = track_key_build_fields((track_key_text){(const char *)tr->title, u8sblen(tr->title)}, artists,
					 tr->ar_size, out);
	xfree(artists);
	return ret;
}

bool track_key_equal(const track_key *a, const track_key *b)
{
	return a->data && b->data && a->hash == b->hash && a->len == b->len && memcmp(a->data, b->data, a->len) == 0;
}

void track_key_free(track_key *key)
{
	xfree(key->data);
	memset(key, 0, sizeof(*key));
}

struct track_key_entry {
	track_key key;
	UT_hash_handle hh;
};

bool track_key_set_contains(const track_key_set *set, const track_key *key)
{
	struct track_key_entry *found;
	HASH_FIND_BYHASHVALUE(hh, set->head, key->data, (unsigned)key->len, key->hash, found);
	return found != NULL;
}

int track_key_set_add(track_key_set *set, const track_key *key)
{
	struct track_key_entry *entry;
	if (track_key_set_contains(set, key))
		return 0;
	entry = xmalloc(sizeof(*entry));
	entry->key = *key;
	entry->key.data = xmalloc(key->len);
	memcpy(entry->key.data, key->data, key->len);
	HASH_ADD_KEYPTR_BYHASHVALUE(hh, set->head, entry->key.data, (unsigned)entry->key.len, entry->key.hash, entry);
	return 1;
}

void track_key_set_free(track_key_set *set)
{
	struct track_key_entry *entry, *tmp;
	HASH_ITER(hh, set->head, entry, tmp)
	{
		HASH_DEL(set->head, entry);
		track_key_free(&entry->key);
		xfree(entry);
	}
}
