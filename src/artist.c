#include "artist.h"
#include "assert.h"
#include "xmalloc.h"
#include <string.h>

static void artist_dump2json(void *ar)
{
	// TODO
}

static dumpType artistDumpType = {
	.dump2json = artist_dump2json,
};

artist *artist_new(const char *name, unsigned int alia_size, const char *alias[])
{
	unsigned int i;
	artist *ar = xmalloc(sizeof(artist) + sizeof(u8s) * alia_size);
	memset(ar,0,sizeof(*ar));
	ar->name = u8snew(name);
	ar->dt = &artistDumpType;

	assert((alia_size > 0 && alias) || (!alia_size && !alias));
	for (i = 0; i < alia_size; ++i) {
		ar->alias[i] = u8snew(alias[i]);
	}
	ar->alia_size = alia_size;
	return ar;
}

void artist_free(artist *ar)
{
	if (ar == NULL)
		return;
	u8sfree(ar->name);
	for (unsigned int i = 0; i < ar->alia_size; ++i)
		u8sfree(ar->alias[i]);
	xfree(ar);
}
