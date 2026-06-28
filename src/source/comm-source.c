#include "comm-source.h"
#include "config.h"


void source_init(source *s, config_t *cfg) {
	assert(cfg);
	s->timeout = cfg->timeout;
	s->max_try = cfg->max_try;
}
