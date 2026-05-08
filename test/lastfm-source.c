#include "global.h"
#include "source.h"
#include "source/lastfm.h"
#include <stdio.h>

int main()
{
	loadGlobalConfig("../example.ini");
	source *s;
	if (lastfm_source_new(&s) < 0) {
		printf("failed to create source");
		return 1;
	}

	recomm_multi(s, 1, NULL, .level = RECOMM_SIMPLE);
	source_free(s);

	globalCurlCleanup();
	globalConfigCleanup();

	return 0;
}
