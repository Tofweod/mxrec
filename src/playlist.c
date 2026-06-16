#include "playlist.h"
#include "da.h"
#include "dump.h"
#include "track.h"
#include "utils/file.h"
#include "xmalloc.h"
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
