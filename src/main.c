#include "comm.h"
#include "monotonic.h"
#include "mxrec.h"
#include "source/lastfm-api.h"
#include "source/lastfm-web.h"
#include "source/ncm.h"
#include "u8string.h"
#include "utils/string.h"
#include "xmalloc.h"
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

config_t config = {0};
config_t cli_config = {0};

source **sources;
int source_count;
const char *default_src_names[MXREC_SOURCE_COUNT - 1];

enum cli_opt_id {
	CLI_OPT_BASE = 256,
#define CONFIG_FIELD(type, name, ...) OPT_##name,
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD
		CLI_OPT_COUNT
};

struct option cli_long_opts[] = {{"config", required_argument, 0, 'c'},
				 {"help", no_argument, 0, 'h'},
#define CONFIG_FIELD(type, name, cli, ...) {cli, required_argument, 0, OPT_##name},
				 CONFIG_FIELD_LIST
#undef CONFIG_FIELD
				 {0, 0, 0, 0}};

enum config_type {
	CONFIG_TYPE_UINT64_T,
	CONFIG_TYPE_U8S,
	CONFIG_TYPE_CSTR,
	CONFIG_TYPE_DOUBLE,
};

#define CONFIG_TYPE_OF(t)                                                                                              \
	_Generic((t *)0,                                                                                               \
		uint64_t *: CONFIG_TYPE_UINT64_T,                                                                      \
		u8s *: CONFIG_TYPE_U8S,                                                                                \
		char **: CONFIG_TYPE_CSTR,                                                                             \
		double *: CONFIG_TYPE_DOUBLE)

static void mxrec_usage()
{
	printf("Usage: %s [options]\n", PROG_NAME);
	printf("Options:\n");
	printf("  -c, --config=<file>          Path to config file\n");
	printf("  -s <source>                  Enable source (use multiple times)\n");
	printf("                               Sources: ");
#define MXREC_SOURCE(name) printf("%s ", #name);
	MXREC_SOURCE_LIST
#undef MXREC_SOURCE
	printf("\n");
	printf("  -h, --help                   Show this help\n");
	printf("\nConfig overrides (--<name>=<value>):\n");
#define CONFIG_FIELD(type, name, cli, dtor, desc) printf("  --%-35s  (%s)\n", cli, desc);
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD
}

static int cli_set_uint64_t(void *field, const char *val)
{
	unsigned long long v;
	if (!string2ull(val, &v))
		return -1;
	*(uint64_t *)field = (uint64_t)v;
	return 0;
}

static int cli_set_u8s(void *field, const char *val)
{
	u8s *dst = field;
	u8s old = *dst;
	*dst = u8snew(val);
	if (old)
		u8sfree(old);
	return *dst ? 0 : -1;
}

static int cli_set_cstr(void *field, const char *val)
{
	char **dst = field;
	char *old = *dst;
	*dst = xstrdup(val);
	xfree(old);
	return *dst ? 0 : -1;
}

static int cli_set_double(void *field, const char *val)
{
	double d;
	if (!string2d(val, strlen(val), &d))
		return -1;
	*(double *)field = d;
	return 0;
}

static int cli_set_field_typed(void *field, const char *val, enum config_type type)
{
	switch (type) {
	case CONFIG_TYPE_UINT64_T:
		return cli_set_uint64_t(field, val);
	case CONFIG_TYPE_U8S:
		return cli_set_u8s(field, val);
	case CONFIG_TYPE_CSTR:
		return cli_set_cstr(field, val);
	case CONFIG_TYPE_DOUBLE:
		return cli_set_double(field, val);
	}
	return -1;
}

static int cli_set_field(config_t *opts, int opt_id, const char *val)
{
	switch (opt_id) {
#define CONFIG_FIELD(type, name, ...)                                                                                  \
	case OPT_##name:                                                                                               \
		return cli_set_field_typed(&opts->name, val, CONFIG_TYPE_OF(type));
		CONFIG_FIELD_LIST
#undef CONFIG_FIELD
	default:
		return -1;
	}
}

int cli_parse_opts(int argc, char **argv, config_t *opts, const char **config_file, const char **src_names,
		   int *src_count)
{
	int opt, idx, nsrc = 0;

	memset(opts, 0, sizeof(*opts));
	*config_file = NULL;

	opterr = 0;

	while ((opt = getopt_long(argc, argv, "c:s:h", cli_long_opts, &idx)) != -1) {
		switch (opt) {
		case 'c':
			*config_file = optarg;
			break;
		case 's':
			if (nsrc < MXREC_SOURCE_COUNT)
				src_names[nsrc++] = optarg;
			break;
		case 'h':
			mxrec_usage();
			return 1;
		case '?':
			printf("Unknown option: %s\n"
			       "Use --help for a list of options\n",
			       argv[optind - 1]);
			return 1;
		default:
			if (cli_set_field(opts, opt, optarg)) {
				printf("invalid value for --%s: %s\n", cli_long_opts[idx].name, optarg);
				return -1;
			}
		}
	}

	*src_count = nsrc;
	return 0;
}

void sources_build(config_t *cfg, const char **names, int count)
{
	int i, ret;
	source_count = count;
	sources = xmalloc(sizeof(*sources) * source_count);

	for (i = 0; i < count && i < MXREC_SOURCE_COUNT; i++) {
#define MXREC_SOURCE(name)                                                                                             \
	if (strcmp(names[i], #name) == 0)                                                                              \
		do {                                                                                                   \
			ret = name##_source_new(&sources[i], cfg);                                                     \
		} while (0);
		ret = -1;
		MXREC_SOURCE_LIST;
		if (ret < 0) {
			panic("failed to create source: %s\n", names[i]);
		}
#undef MXREC_SOURCE
	}
}

static void config_overwrite_uint64_t(void *dest, const void *src)
{
	uint64_t v = *(const uint64_t *)src;
	if (v)
		*(uint64_t *)dest = v;
}

static void config_overwrite_u8s(void *dest, const void *src)
{
	const u8s s = *(const u8s *)src;
	if (s) {
		u8s *d = dest;
		u8s old = *d;
		*d = u8sdup(s);
		if (old)
			u8sfree(old);
	}
}

static void config_overwrite_cstr(void *dest, const void *src)
{
	const char *s = *(const char **)src;
	if (s) {
		char **d = dest;
		char *old = *d;
		*d = xstrdup(s);
		xfree(old);
	}
}

static void config_overwrite_double(void *dest, const void *src)
{
	double v = *(const double *)src;
	if (fabs(v) > 1e-12)
		*(double *)dest = v;
}

static void config_overwrite_field_typed(void *dest, const void *src, enum config_type type)
{
	switch (type) {
	case CONFIG_TYPE_UINT64_T:
		config_overwrite_uint64_t(dest, src);
		break;
	case CONFIG_TYPE_U8S:
		config_overwrite_u8s(dest, src);
		break;
	case CONFIG_TYPE_CSTR:
		config_overwrite_cstr(dest, src);
		break;
	case CONFIG_TYPE_DOUBLE:
		config_overwrite_double(dest, src);
		break;
	}
}

static void config_overwrite(config_t *dest, const config_t *src)
{
	if (!dest || !src)
		return;
#define CONFIG_FIELD(type, name, ...) config_overwrite_field_typed(&dest->name, &src->name, CONFIG_TYPE_OF(type));
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD
}

// manually build
static void default_src_names_build(config_t *cfg)
{
	if (strncmp(cfg->lastfm_method, "api", 4) == 0) {
		default_src_names[0] = "lastfmapi";
	} else if (strncmp(cfg->lastfm_method, "web", 4) == 0) {
		default_src_names[0] = "lastfmweb";
	}
	default_src_names[1] = "ncm";
}

static void mxrec_sources_free()
{
	size_t i = 0;
	for (i = 0; i < source_count; ++i) {
		source_free(sources[i]);
	}
}

int main(int argc, char **argv)
{
	int ret = 0, src_count;
	const char *config_file = NULL;
	const char *src_names[MXREC_SOURCE_COUNT];

	globalCurlInit();

	if (cli_parse_opts(argc, argv, &cli_config, &config_file, src_names, &src_count)) {
		mxrec_cleanup(cleanup, ret, 1);
	}

	if (!load_config(config_file, &config)) {
		mxrec_cleanup(cleanup, ret, 1);
	}

	config_overwrite(&config, &cli_config);
	configfree(&cli_config);

	if (src_count > 0)
		sources_build(&config, src_names, src_count);
	else {
		default_src_names_build(&config);
		sources_build(&config, default_src_names, MXREC_SOURCE_COUNT - 1);
	}

#if MXREC_DEBUG
	size_t i;
	for (i = 0; i < source_count; ++i) {
		printf("source is: %s\n", sources[i]->name);
	}
#endif

cleanup:
	mxrec_sources_free();
	configfree(&config);
	globalCurlCleanup();
	return ret;
}
