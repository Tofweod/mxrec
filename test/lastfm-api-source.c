#include "config.h"
#include "da.h"
#include "dump.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/lastfm-api.h"
#include <stdio.h>

int main()
{
	int ret;
	config_t cfg = {0};
	source *s;
	playlist p;
	const char *configfile = "../config.ini";
	if (!load_config(configfile, &cfg)) {
		return 1;
	}

	if (lastfmapi_source_new(&s, &cfg) < 0) {
		printf("failed to create lastfm api source\n");
		return 1;
	}

	if ((ret = recomm_multi(s, 5, &p, .level = RECOMM_SIMPLE, .use_security = true,
				.lastfmapi_opts.diffusion = cfg.lastfmapi_diffusion_level,
				.lastfmapi_opts.diff_size = cfg.lastfmapi_diffusion_size,
				.lastfmapi_opts.random_lambda = cfg.lastfmapi_random_lambda,
				.lastfmapi_opts.diff_lambda = cfg.lastfmapi_diff_lambda,
				.lastfmapi_opts.score_beta = cfg.lastfmapi_score_beta)) > 0) {
		dump(p->dh, stdout, &p, da_len(p), DUMP2JSON);
		playlist_free(p);
		printf("ret of recomm_multi is %d\n", ret);
	}

	source_free(s);

	configfree(&cfg);
	globalCurlCleanup();

	return 0;
}
