#include "merge-state.h"
#include "assert.h"
#include "da.h"
#include "xmalloc.h"
#include <math.h>
#include <string.h>

int merge_state_init(merge_state *state, playlist *result, size_t wanted, playlist *sources, size_t count,
		     enum sample_method sample)
{
	memset(state, 0, sizeof(*state));
	state->result = result;
	state->wanted = wanted;
	state->source_count = count;
	if ((!sources && count) || (sample != SAMPLE_HEAD && sample != SAMPLE_RANDOM))
		return -1;
	state->sources = count ? xcalloc(count, sizeof(*state->sources)) : NULL;
	for (size_t i = 0; i < count; i++) {
		merge_source_state *dst = &state->sources[i];
		size_t len = da_len(sources[i]);
		track_key_set local = {0};
		dst->items = len ? xcalloc(len, sizeof(*dst->items)) : NULL;
		for (size_t j = 0; j < len; j++) {
			track_key key = {0};
			if (track_key_build(sources[i][j].tr, &key) < 0) {
				track_key_set_free(&local);
				return -1;
			}
			if (track_key_set_add(&local, &key)) {
				dst->items[dst->len++] = (merge_candidate){.item = &sources[i][j], .key = key};
			} else {
				track_key_free(&key);
			}
		}
		track_key_set_free(&local);
		if (sample_cursor_init(&dst->sampler, dst->len, sample) < 0)
			return -1;
	}
	return 0;
}

int merge_state_init_viewers(merge_state *state, playlist *result, size_t wanted, source_result *const *sources,
			     size_t count, enum sample_method sample)
{
	memset(state, 0, sizeof(*state));
	if ((count && !sources) || (sample != SAMPLE_HEAD && sample != SAMPLE_RANDOM))
		return -1;
	state->result = result;
	state->wanted = wanted;
	state->source_count = count;
	state->sources = count ? xcalloc(count, sizeof(*state->sources)) : NULL;
	for (size_t i = 0; i < count; i++) {
		state->sources[i].viewer_mode = true;
		playlist_iterator_init(&state->sources[i].iterator, &sources[i]->viewer, sample);
	}
	return merge_state_refresh(state);
}
int merge_state_refresh(merge_state *state)
{
	for (size_t i = 0; i < state->source_count; i++) {
		merge_source_state *s = &state->sources[i];
		if (!s->viewer_mode)
			continue;
		const playlist_viewer *v = s->iterator.viewer;
		if (v->len == s->len)
			continue;
		s->items = xrealloc(s->items, v->len * sizeof(*s->items));
		while (s->len < v->len) {
			merge_candidate *c = &s->items[s->len];
			memset(c, 0, sizeof(*c));
			c->item = v->unique[s->len];
			if (track_key_build(c->item->tr, &c->key) < 0)
				return -1;
			s->len++;
		}
	}
	return 0;
}

void merge_state_free(merge_state *state)
{
	track_key_set_free(&state->selected);
	track_key_set_free(&state->reservoir_seen);
	if (state->sources) {
		for (size_t i = 0; i < state->source_count; i++) {
			merge_source_state *source = &state->sources[i];
			for (size_t j = 0; j < source->len; j++)
				track_key_free(&source->items[j].key);
			xfree(source->items);
			sample_cursor_free(&source->sampler);
			if (source->viewer_mode)
				playlist_iterator_free(&source->iterator);
		}
	}
	xfree(state->sources);
}

bool merge_source_exhausted(const merge_source_state *source)
{
	if (!source->viewer_mode)
		return source->sampler.cursor >= source->sampler.len;
	const playlist_iterator *it = &source->iterator;
	return it->method == SAMPLE_HEAD ? it->cursor >= it->viewer->len
					 : it->indexed >= it->viewer->len && !it->pending_len;
}

int merge_take_unique(merge_state *state, size_t source_index, size_t wanted)
{
	merge_source_state *source = &state->sources[source_index];
	size_t index, taken = 0;
	while (taken < wanted && da_len(*state->result) < state->wanted) {
		const playitem *item;
		track_key key = {0};
		if (source->viewer_mode) {
			int r = playlist_iterator_next(&source->iterator, &item);
			if (r < 0)
				return -1;
			if (!r)
				break;
			if (state->accept && !state->accept(item, state->accept_context))
				continue;
			if (track_key_build(item->tr, &key) < 0)
				return -1;
			bool added = track_key_set_add(&state->selected, &key);
			track_key_free(&key);
			if (!added)
				continue;
		} else {
			if (!sample_cursor_next(&source->sampler, &index))
				break;
			merge_candidate *candidate = &source->items[index];
			if (state->accept && !state->accept(candidate->item, state->accept_context))
				continue;
			if (!track_key_set_add(&state->selected, &candidate->key))
				continue;
			item = candidate->item;
		}
		da_append(*state->result, *item);
		taken++;
	}
	return (int)taken;
}

int merge_fill_quotas(merge_state *state, const double *weights)
{
	size_t n = state->source_count;
	size_t *quota = n ? xcalloc(n, sizeof(*quota)) : NULL;
	long double *remainders = n ? xcalloc(n, sizeof(*remainders)) : NULL;
	bool any_weight = false;
	int ret = -1;
	for (size_t i = 0; i < n; i++) {
		if (!isfinite(weights[i]) || weights[i] < 0)
			goto cleanup;
		any_weight |= weights[i] > 0;
	}
	if (!any_weight && n)
		goto cleanup;
	while (da_len(*state->result) < state->wanted) {
		size_t remaining = state->wanted - da_len(*state->result), assigned = 0;
		long double total = 0, max_weight = 0;
		for (size_t i = 0; i < n; i++) {
			quota[i] = 0;
			remainders[i] = -1;
			if (!merge_source_exhausted(&state->sources[i]) && weights[i] > max_weight)
				max_weight = weights[i];
		}
		if (!max_weight)
			break;
		for (size_t i = 0; i < n; i++) {
			if (!merge_source_exhausted(&state->sources[i]))
				total += (long double)weights[i] / max_weight;
		}
		for (size_t i = 0; i < n; i++) {
			if (merge_source_exhausted(&state->sources[i]) || weights[i] == 0)
				continue;
			long double exact = remaining * ((long double)weights[i] / max_weight) / total;
			quota[i] = (size_t)floorl(exact);
			if (quota[i] > remaining - assigned)
				quota[i] = remaining - assigned;
			remainders[i] = exact - quota[i];
			assigned += quota[i];
		}
		/* Largest remainders; ties keep source order. */
		while (assigned < remaining) {
			size_t best = n;
			for (size_t i = 0; i < n; i++) {
				if (remainders[i] >= 0 && (best == n || remainders[i] > remainders[best]))
					best = i;
			}
			if (best == n)
				goto cleanup;
			quota[best]++;
			remainders[best] = -1;
			assigned++;
		}
		for (size_t i = 0; i < n; i++)
			if (merge_take_unique(state, i, quota[i]) < 0)
				goto cleanup;
	}
	ret = (int)da_len(*state->result);
cleanup:
	xfree(quota);
	xfree(remainders);
	return ret;
}
