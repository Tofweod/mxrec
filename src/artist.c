#include "artist.h"
#include "xmalloc.h"

static dumpType artistDumpType = {
	// TODO

};

void *artist_new(void)
{
	artist *ar = xmalloc(sizeof(artist));
	if (ar == NULL)
		return NULL;
	ar->dt = &artistDumpType;
	return ar;
}

void artist_free(artist *ar)
{
	if (ar == NULL)
		return;
	xfree(ar->name);
	xfree(ar);
	for (unsigned int i = 0; i < ar->alia_size; ++i)
		u8sfree(ar->alias[i]);
}
