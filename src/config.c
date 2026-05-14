#include "config.h"
#include "assert.h"
#include "iniparser/iniparser.h"
#include "xmalloc.h"
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/// loading config

#define PACKAGE "mxrec"

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
	cfg->lastfm_base_url = xstrdup(iniparser_getstring(d, "lastfm:base-url", NULL));
	cfg->lastfm_username = u8snew(iniparser_getstring(d, "lastfm:username", NULL));
	cfg->lastfm_security_profile = xstrdup(iniparser_getstring(d, "lastfm:security-profile", "chrome116"));

	// lastfm multi_recomm
	cfg->lastfm_mrc_path = xstrdup(iniparser_getstring(d, "lastfm.multi-recomm:path", NULL));
	cfg->lastfm_mrc_method = xstrdup(iniparser_getstring(d, "lastfm.multi-recomm:method", "GET"));
	cfg->lastfm_mrc_parameter = xstrdup(iniparser_getstring(d, "lastfm.multi-recomm:parameter", NULL));
	cfg->lastfm_mrc_accept = xstrdup(iniparser_getstring(d, "lastfm.multi-recomm:accept", NULL));
	cfg->lastfm_mrc_auth = iniparser_getboolean(d, "lastfm.multi-recomm:auth", false);

	iniparser_freedict(d);

	return config_check(cfg);
}

bool load_config(const char *filename, config_t *cfg)
{
	char mxrec_config_dir[PATH_MAX / 2];
	char mxrec_config[PATH_MAX];
	if (filename) {
		return load_config_from_file(filename, cfg);
	}

	char *configDir = getenv("XDG_CONFIG_HOME");
	if (configDir != NULL) {
		sprintf(mxrec_config_dir, "%s/%s/", configDir, PACKAGE);
		mkdir(mxrec_config_dir, 0777);
	} else {
		configDir = getenv("HOME");
		if (configDir != NULL) {
			sprintf(mxrec_config_dir, "%s/%s/", configDir, ".config");
			mkdir(mxrec_config_dir, 0777);

			sprintf(mxrec_config_dir, "%s/%s/%s/", configDir, ".config", PACKAGE);
			mkdir(mxrec_config_dir, 0777);
		} else {
			error("no home directory found");
			return false;
		}
	}

	sprintf(mxrec_config, "%s%s", mxrec_config_dir, "config");
	return load_config_from_file(mxrec_config, cfg);
}

void configfree(config_t *cfg)
{
	if (cfg == NULL)
		return;
#define CONFIG_FIELD(type, name, cleanup) cleanup(cfg->name);
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD
}
