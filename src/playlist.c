#include "playlist.h"
#include "da.h"
#include "dump.h"
#include "track-key.h"
#include "track.h"
#include "utils/file.h"
#include "xmalloc.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

// dump
static void playitemdump2json(FILE *fp, void *ptr)
{
	if (ptr == NULL)
		return;
	playitem *pi = (playitem *)ptr;
	unsigned i;

	fprintf(fp, "{");

	/* track */
	fprintf(fp, "\"track\":");
	if (pi->tr && pi->tr->dh && pi->tr->dh->dump2json)
		pi->tr->dh->dump2json(fp, pi->tr);

	/* urls */
	fprintf(fp, ",\"urls\":[");
	for (i = 0; i < pi->url_size; ++i) {
		if (i > 0)
			fprintf(fp, ",");
		fprintf_json_str(fp, pi->urls[i]);
	}
	fprintf(fp, "]");

	fprintf(fp, "}");
}

static void playlistdump2json(FILE *fp, size_t len, void *ptr)
{
	if (ptr == NULL)
		return;
	size_t i;
	playlist p = *(playlist *)ptr;

	fprintf(fp, "{");

	fprintf(fp, "\"playlist\":[");
	for (i = 0; i < len; ++i) {
		if (i > 0)
			fprintf(fp, ",");
		playitemdump2json(fp, &p[i]);
	}
	fprintf(fp, "]");

	fprintf(fp, "}");
}

static dumpHandle playitemDumpType = {
	.dump2json = playitemdump2json,
	.listdump2json = playlistdump2json,
};

void playitem_init(playitem *pi)
{
	memset(pi, 0, sizeof(*pi));
	pi->dh = &playitemDumpType;
}

int playitem_addurl(playitem *pi, const char *url)
{
	if (url == NULL)
		return 0;

	size_t alloc = pi->url_alloc;
	if (pi->url_size + 1 > alloc) {
		alloc = alloc ? alloc * 2 : 1;
		pi->urls = xrealloc(pi->urls, alloc * sizeof(pi->urls[0]));
		pi->url_alloc = alloc;
	}

	pi->urls[pi->url_size++] = xstrndup(url, MAX_URL_SIZE);
	return 0;
}

void playitem_free(playitem *pi)
{
	unsigned i;
	if (pi == NULL)
		return;

	track_free(pi->tr);
	for (i = 0; i < pi->url_size; ++i)
		xfree(pi->urls[i]);
	xfree(pi->urls);
}

void playlist_free(playlist pl)
{
	if (pl == NULL)
		return;
	size_t i, len = da_len(pl);
	for (i = 0; i < len; ++i) {
		playitem_free(&pl[i]);
	}
	da_free(pl);
}

int playlist_unique_owned(playlist *pl, size_t limit, bool strict)
{
	track_key_set seen = {0};
	size_t len, kept = 0;
	bool *keep;
	int ret = -1;
	if (!pl || limit > INT_MAX)
		return -1;
	len = da_len(*pl);
	if (!len)
		return 0;
	keep = xcalloc(len, sizeof(*keep));
	for (size_t i = 0; i < len && kept < limit; i++) {
		track_key key = {0};
		if (track_key_build((*pl)[i].tr, &key) < 0) {
			if (strict)
				goto cleanup;
			continue;
		}
		keep[i] = track_key_set_add(&seen, &key) != 0;
		kept += keep[i];
		track_key_free(&key);
	}
	kept = 0;
	for (size_t i = 0; i < len; i++) {
		if (keep[i]) {
			if (kept != i) {
				(*pl)[kept] = (*pl)[i];
				memset(&(*pl)[i], 0, sizeof((*pl)[i]));
			}
			kept++;
		} else {
			playitem_free(&(*pl)[i]);
			memset(&(*pl)[i], 0, sizeof((*pl)[i]));
		}
	}
	DAHDR(*pl)->len = kept;
	ret = (int)kept;
cleanup:
	track_key_set_free(&seen);
	xfree(keep);
	return ret;
}
