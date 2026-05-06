#include "source.h"
#include "source/lastfm.h"
#include <stdio.h>

int main()
{
	source *s;
	if (lastfm_source_new(&s) < 0) {
		printf("failed to create source");
		return 1;
	}

	recomm_single(s,NULL);
	source_free(s);
	
	return 0;
}
