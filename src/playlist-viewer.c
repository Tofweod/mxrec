#include "playlist-viewer.h"
#include "assert.h"
#include "da.h"
#include "random.h"
#include "xmalloc.h"
#include <stdint.h>
#include <string.h>

int source_result_append(source_result *r, playlist batch, bool strict)
{
	if (!batch)
		return 0;
	size_t n = da_len(batch);
	/* Validate first, so strict failures never partially append a batch. */
	if (strict)
		for (size_t i = 0; i < n; i++) {
			track_key key = {0};
			if (track_key_build(batch[i].tr, &key) < 0) {
				playlist_free(batch);
				return -1;
			}
			track_key_free(&key);
		}
	playlist_segment *seg = xcalloc(1, sizeof(*seg));
	seg->items = batch;
	if (r->viewer.tail)
		r->viewer.tail->next = seg;
	else
		r->viewer.head = seg;
	r->viewer.tail = seg;
	for (size_t i = 0; i < n; i++) {
		track_key key = {0};
		if (track_key_build(batch[i].tr, &key) < 0)
			continue;
		bool added = track_key_set_add(&r->viewer.seen, &key);
		track_key_free(&key);
		if (!added)
			continue;
		if (r->viewer.len == r->viewer.cap) {
			r->viewer.cap = r->viewer.cap ? r->viewer.cap * 2 : 16;
			r->viewer.unique = xrealloc(r->viewer.unique, r->viewer.cap * sizeof(*r->viewer.unique));
		}
		r->viewer.unique[r->viewer.len++] = &batch[i];
	}
	return 0;
}
void source_result_free(source_result *r)
{
	playlist_segment *seg = r->viewer.head;
	while (seg) {
		playlist_segment *next = seg->next;
		playlist_free(seg->items);
		xfree(seg);
		seg = next;
	}
	track_key_set_free(&r->viewer.seen);
	xfree(r->viewer.unique);
	memset(r, 0, sizeof(*r));
}
void playlist_iterator_init(playlist_iterator *it, const playlist_viewer *viewer, enum sample_method method)
{
	memset(it, 0, sizeof(*it));
	it->viewer = viewer;
	it->method = method;
}
int playlist_iterator_next(playlist_iterator *it, const playitem **item)
{
	if (it->method == SAMPLE_HEAD) {
		if (it->cursor == it->viewer->len)
			return 0;
		*item = it->viewer->unique[it->cursor++];
		return 1;
	}
	while (it->indexed < it->viewer->len) {
		if (it->pending_len == it->pending_cap) {
			it->pending_cap = it->pending_cap ? it->pending_cap * 2 : 16;
			it->pending = xrealloc(it->pending, it->pending_cap * sizeof(*it->pending));
		}
		it->pending[it->pending_len++] = it->viewer->unique[it->indexed++];
	}
	if (!it->pending_len)
		return 0;
	double v;
	if (uniform01(&v) < 0)
		return -1;
	size_t pos = (size_t)(v * it->pending_len);
	*item = it->pending[pos];
	it->pending[pos] = it->pending[--it->pending_len];
	return 1;
}
void playlist_iterator_free(playlist_iterator *it)
{
	xfree(it->pending);
	memset(it, 0, sizeof(*it));
}
