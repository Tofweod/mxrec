#include "config.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/lastfm-api.h"
#include <stdio.h>

int main()
{
	config_t cfg = {0};
	source *s;
	playlist p;
	const char *configfile = "../example.ini";
	if (!load_config(configfile, &cfg)) {
		return 1;
	}

	if (lastfmapi_source_new(&s, &cfg) < 0) {
		printf("failed to create source");
		return 1;
	}

	if (recomm_multi(s, 50, &p, .level = RECOMM_SIMPLE, .use_security = true) > 0) {
		playlist_free(p);
	}

	source_free(s);

	configfree(&cfg);
	globalCurlCleanup();

	return 0;
}
