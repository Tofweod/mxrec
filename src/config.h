#ifndef TOF_MXREC_CONFIG_H
#define TOF_MXREC_CONFIG_H

#include "u8string.h"
#include <limits.h>
#include <stdint.h>

#define CONFIG_FIELD_LIST                                                                                              \
	CONFIG_FIELD(uint64_t, timeout, "timeout", (void), "request timeout (ms)")                                     \
	CONFIG_FIELD(uint64_t, max_try, "max-try", (void), "max retry count")                                          \
	CONFIG_FIELD(char *, sample, "sample", xfree, "final playlist sampling method: head|random")                   \
	CONFIG_FIELD(char *, merge, "merge", xfree,                                                                    \
		     "merge algorithm: uniform|proportional|weighted|priority|round_robin|reservoir")                  \
	CONFIG_FIELD(double, lastfm_merge_weight, "lastfm-merge-weight", (void),                                       \
		     "Last.fm merge weight, used by weighted merge")                                                   \
	CONFIG_FIELD(double, lastfmapi_merge_weight, "lastfmapi-merge-weight", (void),                                \
		     "Last.fm API merge weight override; defaults to lastfm global weight")                       \
	CONFIG_FIELD(double, lastfmweb_merge_weight, "lastfmweb-merge-weight", (void),                                \
		     "Last.fm Web merge weight override; defaults to lastfm global weight")                      \
	CONFIG_FIELD(double, ncm_merge_weight, "ncm-merge-weight", (void), "NCM merge weight, used by weighted merge") \
	CONFIG_FIELD(u8s, lastfm_username, "lastfm-username", u8sfree, "Last.fm username")                             \
	CONFIG_FIELD(char *, lastfm_security_profile, "lastfm-security-profile", xfree, "Last.fm security profile")    \
	CONFIG_FIELD(char *, lastfm_method, "lastfm-method", xfree, "Last.fm API method")                              \
	CONFIG_FIELD(char *, lastfmapi_base_url, "lastfmapi-base-url", xfree, "Last.fm API base URL")                  \
	CONFIG_FIELD(char *, lastfmapi_key, "lastfmapi-key", xfree, "Last.fm API key")                                 \
	CONFIG_FIELD(char *, lastfmapi_period, "lastfmapi-period", xfree, "Last.fm API period")                        \
	CONFIG_FIELD(char *, lastfmapi_strategy, "lastfmapi-strategy", xfree, "Last.fm API strategy")                  \
	CONFIG_FIELD(char *, lastfmapi_sample, "lastfmapi-sample", xfree, "Last.fm API diffusion sample: topn|random") \
	CONFIG_FIELD(uint64_t, lastfmapi_diffusion_level, "lastfmapi-diffusion-level", (void),                         \
		     "Last.fm diffusion level")                                                                        \
	CONFIG_FIELD(uint64_t, lastfmapi_diffusion_size, "lastfmapi-diffusion-size", (void), "Last.fm diffusion size") \
	CONFIG_FIELD(double, lastfmapi_random_lambda, "lastfmapi-random-lambda", (void), "Last.fm random lambda")      \
	CONFIG_FIELD(double, lastfmapi_diff_lambda, "lastfmapi-diff-lambda", (void), "Last.fm diffusion lambda")       \
	CONFIG_FIELD(double, lastfmapi_score_beta, "lastfmapi-score-beta", (void), "Last.fm score beta")               \
	CONFIG_FIELD(char *, lastfmweb_base_url, "lastfmweb-base-url", xfree, "Last.fm web base URL")                  \
	CONFIG_FIELD(char *, lastfmweb_recomm_path, "lastfmweb-recomm-path", xfree, "Last.fm web recomm path")         \
	CONFIG_FIELD(char *, lastfmweb_recomm_method, "lastfmweb-recomm-method", xfree, "Last.fm web recomm method")   \
	CONFIG_FIELD(char *, lastfmweb_recomm_parameter, "lastfmweb-recomm-parameter", xfree,                          \
		     "Last.fm web recomm parameter")                                                                   \
	CONFIG_FIELD(char *, lastfmweb_recomm_accept, "lastfmweb-recomm-accept", xfree, "Last.fm web recomm accept")   \
	CONFIG_FIELD(u8s, ncm_username, "ncm-username", u8sfree, "NCM username")                                       \
	CONFIG_FIELD(char *, ncm_cookie_file, "ncm-cookie-file", xfree, "NCM cookie file path")                        \
	CONFIG_FIELD(char *, ncm_work_dir, "ncm-work-dir", xfree, "NCM work directory")                                \
	CONFIG_FIELD(char *, ncm_bind_method, "ncm-bind-method", xfree, "NCM bind method")                             \
	CONFIG_FIELD(char *, ncm_bind_address, "ncm-bind-address", xfree, "NCM bind address")                          \
	CONFIG_FIELD(uint64_t, ncm_port, "ncm-port", (void), "NCM port number")

typedef struct config_t {
	// TODO recomm level in the future
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
