#include "config.h"
#include "assert.h"
#include "iniparser/iniparser.h"
#include "xmalloc.h"
#include <string.h>

/// loading config

static int config_check(config_t *cfg)
{
	bool ret = true;
#define cfg_check(name)                                        \
	do                                                     \
		if (cfg->name == NULL) {                       \
			error("%s has not be setted.", #name); \
			ret = false;                           \
		}                                              \
	while (0)

	cfg_check(lastfm_base_url);
	cfg_check(lastfm_username);
	cfg_check(lastfm_mrc_path);
	cfg_check(lastfm_mrc_parameter);

	return ret;
}

static inline bool load_config_from_file(const char *filename, config_t *cfg)
{
	dictionary *d;

	d = iniparser_load(filename);
	if (d == NULL) {
		error("failed to load config file:%s", filename);
		return false;
	}

	// TODO
	// general
	cfg->timeout = iniparser_getuint64(d, "general:timeout", 500);
	cfg->max_try = iniparser_getuint64(d, "general:max-try", 3);

	// lastfm
	cfg->lastfm_base_url = xstrdup(iniparser_getstring(d, "lastfm:base_url",
							   "https://www.last.fm"));
	cfg->lastfm_username = u8snew(iniparser_getstring(d, "lastfm:username", NULL));
	cfg->lastfm_user_agent = xstrdup(iniparser_getstring(d, "lastfm:user-agent", NULL));

	// lastfm multi_recomm
	cfg->lastfm_mrc_path = xstrdup(iniparser_getstring(d, "lastfm.multi_recomm:path", NULL));
	cfg->lastfm_mrc_method = xstrdup(iniparser_getstring(d, "lastfm.multi_recomm:method", "GET"));
	cfg->lastfm_mrc_parameter = xstrdup(iniparser_getstring(d, "lastfm.multi_recomm:parameter", NULL));
	cfg->lastfm_mrc_accept = xstrdup(iniparser_getstring(d, "lastfm.multi_recomm:accept", NULL));

	iniparser_freedict(d);

	return config_check(cfg);
}

static const char *default_configs[] = {
	"/etc/mxrec/config.ini",
	"",
	NULL,
};

bool load_config(const char *filename, config_t *cfg)
{
	const char **cur;
	if (filename) {
		return load_config_from_file(filename, cfg);
	}

	cur = default_configs;
	while (*cur) {
		cur++;
	}
	// TODO
	return true;
}

void configfree(config_t *cfg)
{
	if (cfg == NULL)
		return;
#define CONFIG_FIELD(type, name, cleanup) cleanup(cfg->name);
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD
}
