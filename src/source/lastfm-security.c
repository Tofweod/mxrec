#include "lastfm-security.h"
#include "config.h"
#include "xmalloc.h"

lastfm_security __lastfm_security;

int lastfm_security_init(lastfm_security *security, config_t *cfg)
{
	security->profile = xstrdup(cfg->lastfm_security_profile);
	return 0;
}

void lastfm_security_free(lastfm_security *security)
{
	xfree(security->profile);
}
