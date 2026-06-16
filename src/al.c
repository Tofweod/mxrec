#include "al.h"
#include "assert.h"
#include "comm.h"

int merge_sources(playlist *result, size_t wanted, playlist *sources, size_t source_size, enum merge_flag flag,
		  void *data)
{
	if (unlikely(flag == 0)) {
		error("flag of merge sources has not been set yet.");
		return -1;
	}

	if (unlikely(MXREC_BITCOUNT(flag) > 1)) {
		error("merge sources has set multi algorithms by flags, "
		      "please use only one algorithm");
		return -1;
	}

	switch (flag) {
#define MERGE_AL(prefix, f, ...)                                                                                       \
	case MERGE_FLAG_##prefix:                                                                                      \
		return prefix##_merge_sources(result, wanted, sources, source_size, (struct prefix##_params *)data);
		MERGE_AL_LIST
#undef MERGE_AL
	default:
		error("unknown merge algorithm flag: 0x%zx", flag);
		return -1;
	}
}
