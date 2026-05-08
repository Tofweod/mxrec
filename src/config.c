#include "config.h"
#include "assert.h"
#include "comm.h"
#include "iniparser/iniparser.h"
#include "xmalloc.h"
#include <string.h>

/// loading config
static const char *configs[] = {
	"/etc/mxrec/config.ini",
	"",
	NULL,
};

void load_config(config_t **cfg, const char *filename)
{
	const char **cur;
	if (filename) {
		load_config_from_file(filename, cfg);
		return;
	}

	cur = configs;
	while (*cur) {
		cur++;
	}
	// TODO
}

static void config_check(config_t *cfg)
{
	// TODO
#define cfg_check(name)                                        \
	do                                                     \
		if (cfg->name == NULL) {                       \
			panic("%s has not be setted.", #name); \
		}                                              \
	while (0)

	cfg_check(lastfm_base_url);
	cfg_check(lastfm_username);
	cfg_check(lastfm_mrc_path);
	cfg_check(lastfm_mrc_parameter);
}

int load_config_from_file(const char *filename, config_t **cfg)
{
	dictionary *d;
	config_t *c;

	c = xmalloc(sizeof(struct config_t));
	if (unlikely(c == NULL)) {
		return -1;
	}

	d = iniparser_load(filename);
	if (d == NULL) {
		panic("failed to load config file:%s", filename);
	}

	// TODO
	// general
	c->timeout = iniparser_getuint64(d, "general:timeout", 500);
	c->max_try = iniparser_getuint64(d, "general:max-try", 3);

	// lastfm
	c->lastfm_base_url = xstrdup(iniparser_getstring(d, "lastfm:base_url",
							 "https://www.last.fm"));
	c->lastfm_username = u8snew(iniparser_getstring(d, "lastfm:username", NULL));
	c->lastfm_user_agent = xstrdup(iniparser_getstring(d, "lastfm:user-agent", NULL));

	// lastfm multi_recomm
	c->lastfm_mrc_path = xstrdup(iniparser_getstring(d, "lastfm.multi_recomm:path", NULL));
	c->lastfm_mrc_method = xstrdup(iniparser_getstring(d, "lastfm.multi_recomm:method", "GET"));
	c->lastfm_mrc_parameter = xstrdup(iniparser_getstring(d, "lastfm.multi_recomm:parameter", NULL));
	c->lastfm_mrc_accept = xstrdup(iniparser_getstring(d, "lastfm.multi_recomm:accept", NULL));

	iniparser_freedict(d);

	// panic on error
	config_check(c);
	*cfg = c;
	return 0;
}

void configfree(config_t *cfg)
{
#define CONFIG_FIELD(type, name, cleanup) cleanup(cfg->name);
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD

	xfree(cfg);
}
