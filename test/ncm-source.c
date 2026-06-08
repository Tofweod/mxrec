#include "config.h"
#include "source.h"
#include "source/ncm.h"
#include <stdio.h>

int main()
{
	/* int ret; */
	config_t cfg = {0};
	source *s;
	/* playlist p; */
	const char *configfile = "../config.ini";
	if (!load_config(configfile, &cfg)) {
		return 1;
	}

	if (ncm_source_new(&s, &cfg) < 0) {
		printf("failed to cretae ncm source\n");
		return 1;
	}

	ncm_get_auth(s);

	source_free(s);
	configfree(&cfg);

	return 0;
}
