#include "monotonic.h"
#include "assert.h"
#include <curl/curl.h>
#include <stdbool.h>

static bool global_curl_init = false;
static bool global_curl_cleanup = false;

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
