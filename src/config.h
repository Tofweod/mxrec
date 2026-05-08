#ifndef TOF_MXREC_CONFIG_H
#define TOF_MXREC_CONFIG_H

#include "u8string.h"
#include <stdint.h>

#define CONFIG_FIELD_LIST                                 \
	CONFIG_FIELD(uint64_t, timeout, (void))           \
	CONFIG_FIELD(uint64_t, max_try, (void))           \
	CONFIG_FIELD(char *, lastfm_base_url, xfree)      \
	CONFIG_FIELD(char *, lastfm_user_agent, xfree)    \
	CONFIG_FIELD(u8s, lastfm_username, u8sfree)       \
	CONFIG_FIELD(char *, lastfm_mrc_path, xfree)      \
	CONFIG_FIELD(char *, lastfm_mrc_method, xfree)    \
	CONFIG_FIELD(char *, lastfm_mrc_parameter, xfree) \
	CONFIG_FIELD(char *, lastfm_mrc_accept, xfree)

typedef struct config_t {
#define CONFIG_FIELD(type, name, ...) type name;
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD
} config_t;

void load_config(config_t **cfg, const char *filename);

int load_config_from_file(const char *filename, config_t **cfg);

void configfree(config_t *cfg);

#endif // !TOF_MXREC_CONFIG_H
