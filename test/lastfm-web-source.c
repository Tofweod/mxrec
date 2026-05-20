#include "config.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/lastfm-web.h"
#include <stdio.h>

int main()
{
	config_t cfg = {0};
	source *s;
	playlist p;
	playitem pi;
	const char *configfile = "../example.ini";
	if (!load_config(configfile, &cfg)) {
		return 1;
	}

	if (lastfmweb_source_new(&s, &cfg) < 0) {
		printf("failed to create source");
		return 1;
	}

	if (recomm_multi(s, 20, &p, .level = RECOMM_SIMPLE, .use_security = true) > 0) {
		playlist_free(p);
	}

	if (recomm_single(s, &pi, .level = RECOMM_SIMPLE, .use_security = true) == 0) {
		playitem_free(&pi);
	}

	source_free(s);

	configfree(&cfg);
	globalCurlCleanup();

	return 0;
}
