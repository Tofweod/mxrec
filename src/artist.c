#include "artist.h"
#include "xmalloc.h"

static dumpType artistDumpType = {
	// TODO

};

void *artist_new(void)
{
	artist *ar = xmalloc(sizeof(artist));
	ar->alia_size = 0;
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

int aritst_add_alia(artist **ar_ref, const char *alia)
{
	artist *ar = *ar_ref;

	// TODO realloc

	*ar_ref = ar;
	ar->alias[ar->alia_size++] = u8snew(alia);
	return 0;
}
