#include "dedup-helpers.h"
#include "sample.h"
#include <stdint.h>

static int fail_after = -1;
static uint32_t seed = 123;

int __wrap_uniform01(double *out)
{
	if (fail_after == 0)
		return -1;
	if (fail_after > 0)
		fail_after--;
	seed = seed * 1664525u + 1013904223u;
	*out = (double)seed / 4294967296.0;
	return 0;
}

int main(void)
{
	for (int method = SAMPLE_HEAD; method <= SAMPLE_RANDOM; method++) {
		for (size_t len = 0; len <= 64; len++) {
			sample_cursor state;
			bool seen[64] = {0};
			size_t index;
			CHECK(sample_cursor_init(&state, len, method) == 0);
			for (size_t i = 0; i < len; i++) {
				CHECK(sample_cursor_next(&state, &index));
				CHECK(index < len && !seen[index]);
				CHECK(method != SAMPLE_HEAD || index == i);
				seen[index] = true;
			}
			CHECK(!sample_cursor_next(&state, &index));
			sample_cursor_free(&state);
		}
	}
	sample_cursor state;
	CHECK(sample_cursor_init(&state, 5, (enum sample_method)99) < 0);
	sample_cursor_free(&state);
	fail_after = 2;
	CHECK(sample_cursor_init(&state, 8, SAMPLE_RANDOM) < 0);
	CHECK(state.order == NULL && state.len == 0);
	sample_cursor_free(&state);
	puts("sample-test passed");
	return 0;
}
