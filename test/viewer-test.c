#include "al/al-comm.h"
#include "al/merge-state.h"
#include "dedup-helpers.h"
#include "playlist-viewer.h"
#include <stdlib.h>

static void iterators(void)
{
	source_result r = {0};
	playlist_iterator seq, rnd;
	const playitem *item;
	const char *a[] = {"A", "B"}, *b[] = {"B", "C", "D"};
	playlist_iterator_init(&seq, &r.viewer, SAMPLE_HEAD);
	playlist_iterator_init(&rnd, &r.viewer, SAMPLE_RANDOM);
	CHECK(playlist_iterator_next(&seq, &item) == 0);
	CHECK(source_result_append(&r, test_playlist(a, 2), true) == 0);
	CHECK(playlist_iterator_next(&seq, &item) == 1);
	CHECK(!strcmp((const char *)item->tr->title, "A"));
	CHECK(playlist_iterator_next(&rnd, &item) == 1);
	const playitem *first = item;
	CHECK(source_result_append(&r, test_playlist(b, 3), true) == 0);
	CHECK(r.viewer.len == 4);
	const char *expected[] = {"B", "C", "D"};
	for (int i = 0; i < 3; i++) {
		CHECK(playlist_iterator_next(&seq, &item) == 1);
		CHECK(!strcmp((const char *)item->tr->title, expected[i]));
	}
	CHECK(playlist_iterator_next(&seq, &item) == 0);
	const playitem *seen[4] = {first};
	for (int i = 1; i < 4; i++) {
		CHECK(playlist_iterator_next(&rnd, &item) == 1);
		for (int j = 0; j < i; j++)
			CHECK(item != seen[j]);
		seen[i] = item;
	}
	CHECK(playlist_iterator_next(&rnd, &item) == 0);
	playlist_iterator_free(&seq);
	playlist_iterator_free(&rnd);
	source_result_free(&r);
}
static void incremental(merge_source_fp algorithm, enum sample_method sample)
{
	source_result a = {0}, b = {0};
	source_result *src[] = {&a, &b};
	const char *aa[] = {"A"}, *bb[] = {"A", "B"}, *cc[] = {"C"};
	CHECK(!source_result_append(&a, test_playlist(aa, 1), true));
	CHECK(!source_result_append(&b, test_playlist(bb, 2), true));
	playlist out = NULL;
	da_init(out, sizeof(*out));
	merge_state state;
	CHECK(!merge_state_init_viewers(&state, &out, 3, src, 2, sample));
	double weights[] = {1, 1};
	struct weighted_params wp = {
		.base = {.merge_source = algorithm, .sample = sample}, .weights = weights, .count = 2};
	struct merge_params *params = &wp.base;
	CHECK(params->merge_source(&state, params) == 2);
	CHECK(!source_result_append(&a, test_playlist(cc, 1), true));
	CHECK(!merge_state_refresh(&state));
	CHECK(params->merge_source(&state, params) == 3);
	test_unique(out);
	merge_state_free(&state);
	da_free(out);
	source_result_free(&a);
	source_result_free(&b);
}
int main(void)
{
	iterators();
	merge_source_fp methods[] = {weighted_merge_sources, uniform_merge_sources,	proportional_merge_sources,
				     priority_merge_sources, round_robin_merge_sources, reservoir_merge_sources};
	for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
		incremental(methods[i], SAMPLE_HEAD);
		incremental(methods[i], SAMPLE_RANDOM);
	}
	return 0;
}
