#include "config.h"
#include "monotonic.h"
#include "source.h"
#include "source/lastfm.h"
#include <stdio.h>

int main()
{
	config_t cfg;
	source *s;
	const char *configfile = "../example.ini";
	if (!load_config(configfile, &cfg)) {
		printf("failed to load config file:%s\n", configfile);
		return 1;
	}

	if (lastfm_source_new(&s, &cfg) < 0) {
		printf("failed to create source");
		return 1;
	}

	recomm_multi(s, 1, NULL, .level = RECOMM_SIMPLE);
	source_free(s);

	globalCurlCleanup();

	return 0;
}
