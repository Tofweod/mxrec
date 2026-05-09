#ifndef TOF_MXREC_COMM_SOURCE_H
#define TOF_MXREC_COMM_SOURCE_H

#include "source.h"

typedef struct source source;
typedef struct config_t config_t;

// function declaition
#define FUNCTION_FIELD_LIST                                                     \
	FUNCTION_FIELD(void, source_destroy, void *)                            \
	FUNCTION_FIELD(int, recomm_single, source *, playentry *, recomm_option) \
	FUNCTION_FIELD(int, recomm_multi, source *, size_t, playlist *, recomm_option)

#endif // !TOF_MXREC_COMM_SOURCE_H
