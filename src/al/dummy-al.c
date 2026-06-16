#include "al.h"
#include "assert.h"
#include "comm.h"

int dummy_merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size,
			struct dummy_params *params mxrec_unused)
{
	error("using dummy merge sources algorithm, "
	      "please change to a concrete implementation.");
	return 0;
}
