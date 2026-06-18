#include "al-comm.h"
#include "assert.h"
#include "comm.h"

int dummy_merge_sources(playlist *result mxrec_unused, size_t wanted mxrec_unused, playlist *sources mxrec_unused,
			size_t source_size mxrec_unused, struct merge_params *params mxrec_unused)
{
	error("using dummy merge sources algorithm, "
	      "please change to a concrete implementation.");
	return -1;
}
