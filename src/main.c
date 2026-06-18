#include "comm.h"
#include "config.h"
#include "monotonic.h"
#include "mxrec.h"
#include "u8string.h"
#include "utils/string.h"
#include "xmalloc.h"
#include <getopt.h>
#include <stdio.h>

config_t config = {0};
config_t cli_config = {0};

enum cli_opt_id {
#define CONFIG_FIELD(type, name, ...) OPT_##name,
	CONFIG_FIELD_LIST
#undef CONFIG_FIELD
		OPT__COUNT
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
	CONFIG_TYPE_UNSIGNED,
	CONFIG_TYPE_DOUBLE,
	CONFIG_TYPE_UINT16_T,
};

#define CLI_TYPE_OF(t)                                                                                                 \
	_Generic((t *)0,                                                                                               \
		uint64_t *: CONFIG_TYPE_UINT64_T,                                                                      \
		u8s *: CONFIG_TYPE_U8S,                                                                                \
		char **: CONFIG_TYPE_CSTR,                                                                             \
		unsigned *: CONFIG_TYPE_UNSIGNED,                                                                      \
		double *: CONFIG_TYPE_DOUBLE,                                                                          \
		uint16_t *: CONFIG_TYPE_UINT16_T)

static void mxrec_usage()
{
	printf("Usage: %s [options]\n", PROG_NAME);
	printf("Options:\n");
	printf("  -c, --config=<file>          Path to config file\n");
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

static int cli_set_unsigned(void *field, const char *val)
{
	unsigned long long v;
	if (!string2ull(val, &v))
		return -1;
	*(unsigned *)field = (unsigned)v;
	return 0;
}

static int cli_set_double(void *field, const char *val)
{
	double d;
	if (!string2d(val, strlen(val), &d))
		return -1;
	*(double *)field = d;
	return 0;
}

static int cli_set_uint16_t(void *field, const char *val)
{
	unsigned long long v;
	if (!string2ull(val, &v) || v > UINT16_MAX)
		return -1;
	*(uint16_t *)field = (uint16_t)v;
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
	case CONFIG_TYPE_UNSIGNED:
		return cli_set_unsigned(field, val);
	case CONFIG_TYPE_DOUBLE:
		return cli_set_double(field, val);
	case CONFIG_TYPE_UINT16_T:
		return cli_set_uint16_t(field, val);
	}
	return -1;
}

static int cli_set_field(config_t *opts, int opt_id, const char *val)
{
	switch (opt_id) {
#define CONFIG_FIELD(type, name, ...)                                                                                  \
	case OPT_##name:                                                                                               \
		return cli_set_field_typed(&opts->name, val, CLI_TYPE_OF(type));
		CONFIG_FIELD_LIST
#undef CONFIG_FIELD
	default:
		return -1;
	}
}

int cli_parse_opts(int argc, char **argv, config_t *opts, const char **config_file)
{
	int opt, idx;

	memset(opts, 0, sizeof(*opts));
	*config_file = NULL;

	opterr = 0;

	while ((opt = getopt_long(argc, argv, "c:h", cli_long_opts, &idx)) != -1) {
		switch (opt) {
		case 'c':
			*config_file = optarg;
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

	return 0;
}

void sources_init()
{
	size_t i;
	for (;;) {
#define MXREC_SOURCE(...)
		MXREC_SOURCE_LIST;
	}
}

static void config_overwrite(config_t *dest, const config_t *src)
{
	if (!dest || !src)
		return;
	// TODO
}

int main(int argc, char **argv)
{
	int ret = 0;
	const char *config_file = NULL;

	globalCurlInit();

	if (cli_parse_opts(argc, argv, &cli_config, &config_file)) {
		mxrec_cleanup(cleanup, ret, 1);
	}

	if (!load_config(config_file, &config)) {
		mxrec_cleanup(cleanup, ret, 1);
	}

	config_overwrite(&config, &cli_config);
	configfree(&cli_config);

	/* sources_init(); */

	configfree(&config);
cleanup:
	globalCurlCleanup();
	return ret;
}
