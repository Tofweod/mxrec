#include "playlist.h"
#include "da.h"
#include "track.h"
#include "xmalloc.h"
#include <stddef.h>
#include <string.h>

static dumpType playlistDumpType = {

};

void playitem_init(playitem *pi)
{
	memset(pi, 0, sizeof(*pi));
	pi->dt = &playlistDumpType;
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

	track_free(pi->tr);
	for (i = 0; i < pi->url_size; ++i)
		xfree(pi->urls[i]);
	xfree(pi->urls);
}

void playlist_free(playlist pl)
{
	size_t i, len = da_len(pl);
	for (i = 0; i < len; ++i) {
		playitem_free(&pl[i]);
	}
	da_free(pl);
}
