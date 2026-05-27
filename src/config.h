#ifndef TOF_MXREC_CONFIG_H
#define TOF_MXREC_CONFIG_H

#include "u8string.h"
#include <limits.h>
#include <stdint.h>

#define CONFIG_FIELD_LIST                                                                                              \
	CONFIG_FIELD(uint64_t, timeout, (void))                                                                        \
	CONFIG_FIELD(uint64_t, max_try, (void))                                                                        \
	CONFIG_FIELD(u8s, lastfm_username, u8sfree)                                                                    \
	CONFIG_FIELD(char *, lastfm_security_profile, xfree)                                                           \
	CONFIG_FIELD(char *, lastfm_method, xfree)                                                                     \
	CONFIG_FIELD(char *, lastfmapi_base_url, xfree)                                                                \
	CONFIG_FIELD(char *, lastfmapi_key, xfree)                                                                     \
	CONFIG_FIELD(char *, lastfmapi_period, xfree)                                                                  \
	CONFIG_FIELD(char *, lastfmapi_strategy, xfree)                                                                \
	CONFIG_FIELD(unsigned, lastfmapi_diffusion_level, (void))                                                      \
	CONFIG_FIELD(unsigned, lastfmapi_diffusion_size, (void))                                                       \
	CONFIG_FIELD(char *, lastfmweb_base_url, xfree)                                                                \
	CONFIG_FIELD(char *, lastfmweb_recomm_path, xfree)                                                             \
	CONFIG_FIELD(char *, lastfmweb_recomm_method, xfree)                                                           \
	CONFIG_FIELD(char *, lastfmweb_recomm_parameter, xfree)                                                        \
	CONFIG_FIELD(char *, lastfmweb_recomm_accept, xfree)                                                           \
	CONFIG_FIELD(char *, ncm_cookie, xfree)                                                                        \
	CONFIG_FIELD(char *, ncm_bind_method, xfree)                                                                   \
	CONFIG_FIELD(char *, ncm_bind_address, xfree)                                                                  \
	CONFIG_FIELD(uint16_t, ncm_port, (void))

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
