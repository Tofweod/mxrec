#ifndef TOF_MXREC_CONFIG_H
#define TOF_MXREC_CONFIG_H

#include "u8string.h"
#include <limits.h>
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

bool load_config(const char *filename, config_t *cfg);

/** it simply free fields in config struct, if config itself is heap memory,
 *  please free it manually
 */
void configfree(config_t *cfg);

#endif // !TOF_MXREC_CONFIG_H
