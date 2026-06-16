#include "config.h"
#include "source.h"
#include "source/ncm.h"
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

	if (ncm_source_new(&s, &cfg) < 0) {
		printf("failed to cretae ncm source\n");
		return 1;
	}

	if ((ret = recomm_multi(s, 20, &p, .ncm_opts.daily_recomm_fresh = true)) > 0) {
		dump(p->dh, stdout, &p, ret, DUMP2JSON);
		playlist_free(p);
		printf("ret of recomm_multi is %d\n", ret);
	}

	source_free(s);
	configfree(&cfg);

	return 0;
}
