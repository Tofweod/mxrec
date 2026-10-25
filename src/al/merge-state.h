#ifndef TOF_MXREC_MERGE_STATE_H
#define TOF_MXREC_MERGE_STATE_H

#include "al.h"
#include "track-key.h"

typedef struct merge_candidate {
	const playitem *item;
	track_key key;
} merge_candidate;

typedef struct merge_source_state {
	merge_candidate *items;
	size_t len;
	sample_cursor sampler;
} merge_source_state;

typedef struct merge_state {
	merge_source_state *sources;
	size_t source_count;
	size_t wanted;
	playlist *result;
	track_key_set selected;
} merge_state;

int merge_state_init(merge_state *state, playlist *result, size_t wanted, playlist *sources, size_t count,
		     enum sample_method sample);
void merge_state_free(merge_state *state);
bool merge_source_exhausted(const merge_source_state *source);
int merge_take_unique(merge_state *state, size_t source_index, size_t wanted);
/* Positive weights determine quotas; zero weights exclude a source. */
int merge_fill_quotas(merge_state *state, const double *weights);

#endif
