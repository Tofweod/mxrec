#include "track.h"
#include "artist.h"
#include "assert.h"
#include "xmalloc.h"
#include <string.h>

static void trackdump2json(void *tr)
{
	// TODO
}

static dumpType trackDumpType = {
	.dump2json = trackdump2json,
	.listdump2json = NULL,
};

void *track_new(const char *title, const char *album, unsigned int ar_size, artist **ars, unsigned int alia_size,
		const char *alias[])
{
	unsigned int i;
	track *tr = xmalloc(sizeof(track) + sizeof(u8s) * alia_size);
	memset(tr, 0, sizeof(*tr));
	tr->dt = &trackDumpType;

	tr->title = u8snew(title);
	tr->album = u8snew(album);

	assert(ar_size > 0 && ars);
	tr->ar_size = ar_size;
	tr->artists = ars;

	assert((alia_size > 0 && alias) || (!alia_size && !alias));
	for (i = 0; i < alia_size; ++i)
		tr->alias[i] = u8snew(alias[i]);
	tr->alia_size = alia_size;

	return tr;
}

void track_free(track *tr)
{
	unsigned int i;
	if (tr == NULL)
		return;
	u8sfree(tr->title);
	u8sfree(tr->album);
	for (i = 0; i < tr->ar_size; ++i)
		artist_free(tr->artists[i]);
	xfree(tr->artists);

	for (i = 0; i < tr->alia_size; ++i)
		u8sfree(tr->alias[i]);
	xfree(tr);
}
