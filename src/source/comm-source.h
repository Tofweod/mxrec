#ifndef TOF_MXREC_COMM_SOURCE_H
#define TOF_MXREC_COMM_SOURCE_H

#include "source.h"

typedef struct source source;
typedef struct config_t config_t;

// function declaition
#define FUNCTION_FIELD_LIST                                                                                            \
	FUNCTION_FIELD(void, source_destroy, void *)                                                                   \
	FUNCTION_FIELD(int, recomm_single, source *, playitem *, recomm_option)                                        \
	FUNCTION_FIELD(int, recomm_multi, source *, size_t, playlist *, recomm_option)                                 \
	FUNCTION_FIELD(void, security_handle, source *s)                                                               \
	FUNCTION_FIELD(bool, config_check, source *s)

void source_init(source *s, config_t *cfg);

#endif // !TOF_MXREC_COMM_SOURCE_H
