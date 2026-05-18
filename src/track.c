#include "track.h"
#include "artist.h"
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

void *track_new(void)
{
	track *tr = xmalloc(sizeof(track));

	memset(tr, 0, sizeof(*tr));
	tr->dt = &trackDumpType;
	return tr;
}
void track_free(track *tr)
{
	if (tr == NULL)
		return;
	xfree(tr->title);
	xfree(tr->album);
	for (size_t i = 0; i < tr->ar_size; ++i)
		artist_free(tr->artists[i]);

	for (size_t i = 0; i < tr->alia_size; ++i)
		u8sfree(tr->alias[i]);

	xfree(tr);
}
