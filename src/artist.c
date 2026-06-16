#include "artist.h"
#include "assert.h"
#include "dump.h"
#include "utils/file.h"
#include "xmalloc.h"
#include <string.h>

static void artist_dump2json(FILE *fp, void *ptr)
{
	if (ptr == NULL)
		return;
	artist *ar = (artist *)ptr;
	unsigned i;

	fprintf(fp, "{");

	/* name */
	fprintf(fp, "\"name\":");
	fprintf_json_str(fp, (const char *)ar->name);

	/* alias */
	fprintf(fp, ",\"alias\":[");
	for (i = 0; i < ar->alia_size; ++i) {
		if (i > 0)
			fprintf(fp, ",");
		fprintf_json_str(fp, (const char *)ar->alias[i]);
	}
	fprintf(fp, "]");

	fprintf(fp, "}");
}

static dumpHandle artistDumpType = {
	.dump2json = artist_dump2json,
};

artist *artist_new(const char *name, unsigned int alia_size, const char *alias[])
{
	unsigned int i;
	artist *ar = xmalloc(sizeof(artist) + sizeof(u8s) * alia_size);
	memset(ar, 0, sizeof(*ar));
	ar->name = u8snew(name);
	ar->dh = &artistDumpType;

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
