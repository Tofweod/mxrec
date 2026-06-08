#include "config.h"
#include "assert.h"
#include "iniparser/src/iniparser.h"
#include "xmalloc.h"
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/// loading config

#define PACKAGE "mxrec"

static void removeURLEndSlash(char *url)
{
	size_t len = strlen(url);
	if (len > 0 && url[len - 1] == '/') {
		url[len - 1] = '\0';
	}
}

static inline bool load_config_from_file(const char *filename, config_t *cfg)
{
	dictionary *d;

	d = iniparser_load(filename);
	if (d == NULL) {
		error("failed to load config file:%s", filename);
		return false;
	}

	// TODO set default value here
	// general
	cfg->timeout = iniparser_getuint64(d, "general:timeout", 500);
	cfg->max_try = iniparser_getuint64(d, "general:max-try", 3);

	// lastfm
	cfg->lastfm_method = xstrdup(iniparser_getstring(d, "lastfm.method", NULL));
	cfg->lastfm_username = u8snew(iniparser_getstring(d, "lastfm:username", NULL));
	cfg->lastfm_security_profile = xstrdup(iniparser_getstring(d, "lastfm:security-profile", "chrome116"));

	// lastfm api
	cfg->lastfmapi_base_url = xstrdup(iniparser_getstring(d, "lastfm.api:base-url", NULL));
	cfg->lastfmapi_key = xstrdup(iniparser_getstring(d, "lastfm.api:key", NULL));
	cfg->lastfmapi_period = xstrdup(iniparser_getstring(d, "lastfm.api:period", NULL));
	cfg->lastfmapi_diffusion_level = iniparser_getuint64(d, "lastfm.api:diffusion", 0);
	cfg->lastfmapi_diffusion_size = iniparser_getuint64(d, "lastfm.api:diff-size", 5);
	cfg->lastfmapi_strategy = xstrdup(iniparser_getstring(d, "lastfm.api:strategy", NULL));
	cfg->lastfmapi_sample = xstrdup(iniparser_getstring(d, "lastfm.api:sample", NULL));
	cfg->lastfmapi_random_lambda = iniparser_getdouble(d, "lastfm.api:random-sample-lambda", 0.0);
	cfg->lastfmapi_diff_lambda = iniparser_getdouble(d, "lastfm.api:diffusion-lambda", 1.0);
	cfg->lastfmapi_score_beta = iniparser_getdouble(d, "lastfm.api:score-beta", 10.0);

	// lastfm web
	cfg->lastfmweb_base_url = xstrdup(iniparser_getstring(d, "lastfm.web:base-url", NULL));
	removeURLEndSlash(cfg->lastfmweb_base_url);
	cfg->lastfmweb_recomm_path = xstrdup(iniparser_getstring(d, "lastfm.web:recomm-path", NULL));
	cfg->lastfmweb_recomm_method = xstrdup(iniparser_getstring(d, "lastfm.web:recomm-method", "GET"));
	cfg->lastfmweb_recomm_parameter = xstrdup(iniparser_getstring(d, "lastfm.web:recomm-parameter", NULL));
	cfg->lastfmweb_recomm_accept = xstrdup(iniparser_getstring(d, "lastfm.web:recomm-accept", NULL));

	// ncm
	cfg->ncm_work_dir = xstrdup(iniparser_getstring(d, "ncm:work-dir", NULL));
	cfg->ncm_cookie = xstrdup(iniparser_getstring(d, "ncm:cookie", NULL));
	cfg->ncm_bind_method = xstrdup(iniparser_getstring(d, "ncm:bind-method", "http"));
	cfg->ncm_bind_address = xstrdup(iniparser_getstring(d, "ncm:bind-address", "127.0.0.1"));
	cfg->ncm_port = iniparser_getuint64(d, "ncm:port", 9900);

	iniparser_freedict(d);

	return true;
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
