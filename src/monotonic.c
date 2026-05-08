#include "assert.h"
#include "config.h"
#include "global.h"
#include <curl/curl.h>
#include <stdbool.h>

static bool global_curl_init = false;
static bool global_curl_cleanup = false;

static config_t *mxrec_glocal_config = NULL;

void globalCurlInit(void)
{
	if (global_curl_cleanup)
		panic("global curl has been cleanup");
	if (!global_curl_init) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		global_curl_init = true;
	}
}

void globalCurlCleanup(void)
{
	if (!global_curl_init)
		panic("cleanup golbal curl before initialization");
	curl_global_cleanup();
	global_curl_cleanup = true;
}

config_t *getGlobalConfig(void)
{
	if (mxrec_glocal_config == NULL) {
		panic("global config has not been loaded.");
	}
	return mxrec_glocal_config;
}

void globalConfigCleanup(void)
{
	if (mxrec_glocal_config != NULL)
		configfree(mxrec_glocal_config);
}

void globalCleanup(void)
{
	globalCurlCleanup();
	globalConfigCleanup();
}

void loadGlobalConfig(const char *filename)
{
	if (mxrec_glocal_config == NULL)
		load_config(&mxrec_glocal_config, filename);
}
