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
	if (!url)
		return;
	size_t len = strlen(url);
	if (len > 0 && url[len - 1] == '/') {
		url[len - 1] = '\0';
	}
}

static const char *config_get_string_or_default(dictionary *d, const char *key, const char *def)
{
	const char *value = iniparser_getstring(d, key, def);
	return value && value[0] ? value : def;
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
	cfg->sample = xstrdup(config_get_string_or_default(d, "general:sample", "head"));
	cfg->merge = xstrdup(config_get_string_or_default(d, "general:merge", "round_robin"));
	cfg->lastfm_merge_weight = iniparser_getdouble(d, "lastfm:merge_weight", 1.0);
	cfg->lastfmapi_merge_weight =
		iniparser_getdouble(d, "lastfm.api:merge-weight", cfg->lastfm_merge_weight);
	cfg->lastfmweb_merge_weight =
		iniparser_getdouble(d, "lastfm.web:merge-weight", cfg->lastfm_merge_weight);
	cfg->ncm_merge_weight = iniparser_getdouble(d, "ncm:merge_weight", 1.0);

	// lastfm
	cfg->lastfm_method = xstrdup(config_get_string_or_default(d, "lastfm:method", NULL));
	cfg->lastfm_username = u8snew(config_get_string_or_default(d, "lastfm:username", NULL));
	cfg->lastfm_security_profile = xstrdup(config_get_string_or_default(d, "lastfm:security-profile", "chrome116"));

	// lastfm api
	cfg->lastfmapi_base_url = xstrdup(config_get_string_or_default(d, "lastfm.api:base-url", NULL));
	cfg->lastfmapi_key = xstrdup(config_get_string_or_default(d, "lastfm.api:key", NULL));
	cfg->lastfmapi_period = xstrdup(config_get_string_or_default(d, "lastfm.api:period", NULL));
	cfg->lastfmapi_diffusion_level = iniparser_getuint64(d, "lastfm.api:diffusion", 0);
	cfg->lastfmapi_diffusion_size = iniparser_getuint64(d, "lastfm.api:diff-size", 5);
	cfg->lastfmapi_strategy = xstrdup(config_get_string_or_default(d, "lastfm.api:strategy", NULL));
	cfg->lastfmapi_sample = xstrdup(config_get_string_or_default(d, "lastfm.api:sample", NULL));
	cfg->lastfmapi_random_lambda = iniparser_getdouble(d, "lastfm.api:random-sample-lambda", 0.0);
	cfg->lastfmapi_diff_lambda = iniparser_getdouble(d, "lastfm.api:diffusion-lambda", 1.0);
	cfg->lastfmapi_score_beta = iniparser_getdouble(d, "lastfm.api:score-beta", 10.0);

	// lastfm web
	cfg->lastfmweb_base_url = xstrdup(config_get_string_or_default(d, "lastfm.web:base-url", NULL));
	removeURLEndSlash(cfg->lastfmweb_base_url);
	cfg->lastfmweb_recomm_path = xstrdup(config_get_string_or_default(d, "lastfm.web:recomm-path", NULL));
	cfg->lastfmweb_recomm_method = xstrdup(config_get_string_or_default(d, "lastfm.web:recomm-method", "GET"));
	cfg->lastfmweb_recomm_parameter = xstrdup(config_get_string_or_default(d, "lastfm.web:recomm-parameter", NULL));
	cfg->lastfmweb_recomm_accept = xstrdup(config_get_string_or_default(d, "lastfm.web:recomm-accept", NULL));

	// ncm
	cfg->ncm_username = u8snew(config_get_string_or_default(d, "ncm:username", NULL));
	cfg->ncm_work_dir = xstrdup(config_get_string_or_default(d, "ncm:work-dir", NULL));
	cfg->ncm_cookie_file = xstrdup(config_get_string_or_default(d, "ncm:cookie-file", NULL));
	cfg->ncm_bind_method = xstrdup(config_get_string_or_default(d, "ncm:bind-method", "http"));
	cfg->ncm_bind_address = xstrdup(config_get_string_or_default(d, "ncm:bind-address", NULL));
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
#define CONFIG_FIELD(type, name, _, cleanup, ...) cleanup(cfg->name);
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD
}
