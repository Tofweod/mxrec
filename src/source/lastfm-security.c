#include "config.h"
#include "lastfm-comm.h"
#include "xmalloc.h"

lastfm_security __lastfm_security;

static bool __security_has_created = false;
static bool __security_has_destroy = false;

int lastfm_security_init(config_t *cfg)
{
	if (!__security_has_created) {
		__lastfm_security.profile = xstrdup(cfg->lastfm_security_profile);
		__security_has_created = true;
	}
	return 0;
}

void lastfm_security_free()
{
	if (__security_has_created && !__security_has_destroy) {
		xfree(__lastfm_security.profile);
		__security_has_destroy = true;
	}
}
