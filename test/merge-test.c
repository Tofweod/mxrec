#include "al/al-comm.h"
#include "dedup-helpers.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

static int fail_after = -1, random_calls;
static uint32_t seed = 123;

int __wrap_uniform01(double *out)
{
	if (fail_after == 0)
		return -1;
	if (fail_after > 0)
		fail_after--;
	random_calls++;
	seed = seed * 1664525u + 1013904223u;
	*out = (double)seed / 4294967296.0;
	return 0;
}

static const merge_source_fp algorithms[] = {
	uniform_merge_sources,	proportional_merge_sources, weighted_merge_sources,
	priority_merge_sources, round_robin_merge_sources,  reservoir_merge_sources,
};

static playlist run(merge_source_fp algorithm, enum sample_method sample, playlist *inputs, size_t count, size_t wanted,
		    double *weights, size_t expected)
{
	playitem snapshots[3][12];
	CHECK(count <= 3);
	for (size_t i = 0; i < count; i++) {
		CHECK(da_len(inputs[i]) <= 12);
		if (da_len(inputs[i]))
			memcpy(snapshots[i], inputs[i], da_len(inputs[i]) * sizeof(playitem));
	}
	struct weighted_params p = {
		.base = {.merge_source = algorithm, .sample = sample}, .weights = weights, .count = count};
	playlist out = NULL;
	da_init(out, sizeof(*out));
	CHECK(merge_sources(&out, wanted, inputs, count, &p.base) == (int)expected);
	CHECK(da_len(out) == expected);
	test_unique(out);
	for (size_t i = 0; i < count; i++) {
		if (da_len(inputs[i]))
			CHECK(memcmp(snapshots[i], inputs[i], da_len(inputs[i]) * sizeof(playitem)) == 0);
	}
	for (size_t j = 0; j < da_len(out); j++) {
		bool found = false;
		for (size_t i = 0; i < count; i++) {
			for (size_t k = 0; k < da_len(inputs[i]); k++)
				found |= memcmp(&out[j], &inputs[i][k], sizeof(playitem)) == 0;
		}
		CHECK(found);
	}
	return out;
}

static void all_algorithms(playlist *inputs, size_t count, size_t unique)
{
	double weights[] = {1, 1, 1};
	for (size_t a = 0; a < sizeof(algorithms) / sizeof(algorithms[0]); a++) {
		for (int sample = SAMPLE_HEAD; sample <= SAMPLE_RANDOM; sample++) {
			for (size_t wanted = 0; wanted <= unique + 2; wanted++) {
				playlist out = run(algorithms[a], sample, inputs, count, wanted, weights,
						   wanted < unique ? wanted : unique);
				da_free(out);
			}
		}
	}
}

static void quota_counts(merge_source_fp algorithm, size_t wanted, double *weights, const size_t *expected)
{
	const char *a[] = {"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7"};
	const char *b[] = {"B0", "B1", "B2", "B3"};
	const char *c[] = {"C0", "C1", "C2", "C3"};
	playlist inputs[] = {test_playlist(a, 8), test_playlist(b, 4), test_playlist(c, 4)};
	playlist out = run(algorithm, SAMPLE_HEAD, inputs, 3, wanted, weights, wanted);
	size_t actual[3] = {0};
	for (size_t j = 0; j < da_len(out); j++) {
		for (size_t i = 0; i < 3; i++) {
			for (size_t k = 0; k < da_len(inputs[i]); k++)
				actual[i] += out[j].tr == inputs[i][k].tr;
		}
	}
	CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
	da_free(out);
	for (size_t i = 0; i < 3; i++)
		playlist_free(inputs[i]);
}

static void boundaries(void)
{
	const char *a[] = {"X"}, *b[] = {"X", "Y", "Z", "W"};
	playlist inputs[] = {test_playlist(a, 1), test_playlist(b, 4), NULL};
	double weights[] = {100, 1, 0};
	playlist out = run(weighted_merge_sources, SAMPLE_HEAD, inputs, 3, 4, weights, 4);
	da_free(out);
	weights[0] = DBL_MAX;
	weights[1] = DBL_MIN;
	out = run(weighted_merge_sources, SAMPLE_HEAD, inputs, 3, 4, weights, 4);
	da_free(out);
	weights[0] = 1;
	weights[1] = 0;
	out = run(weighted_merge_sources, SAMPLE_HEAD, inputs, 3, 4, weights, 1);
	da_free(out);

	struct weighted_params p = {.base = {.merge_source = weighted_merge_sources, .sample = SAMPLE_HEAD},
				    .weights = weights,
				    .count = 3};
	out = NULL;
	da_init(out, sizeof(*out));
	double bad[] = {0, -1, NAN, INFINITY};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		weights[0] = bad[i];
		CHECK(merge_sources(&out, 4, inputs, 3, &p.base) < 0 && da_len(out) == 0);
	}
	weights[0] = 1;
	p.count = 2;
	CHECK(merge_sources(&out, 4, inputs, 3, &p.base) < 0);
	p.count = 3;
	p.base.sample = (enum sample_method)99;
	CHECK(merge_sources(&out, 4, inputs, 3, &p.base) < 0);
	p.base.sample = SAMPLE_RANDOM;
	fail_after = 0;
	CHECK(merge_sources(&out, 4, inputs, 3, &p.base) < 0 && da_len(out) == 0);
	p.base.sample = SAMPLE_HEAD;
	p.base.merge_source = reservoir_merge_sources;
	CHECK(merge_sources(&out, 1, inputs, 3, &p.base) < 0 && da_len(out) == 0);
	fail_after = -1;
	p.base.merge_source = priority_merge_sources;
	CHECK(merge_sources(&out, (size_t)INT_MAX + 1, inputs, 3, &p.base) < 0);
	CHECK(merge_sources(&out, 4, NULL, 3, &p.base) < 0);
	da_free(out);
	playlist_free(inputs[0]);
	playlist_free(inputs[1]);

	/* Initialization failure after earlier valid keys must release those keys. */
	const char *invalid[] = {"X", " "};
	inputs[0] = test_playlist(invalid, 2);
	inputs[1] = NULL;
	out = NULL;
	da_init(out, sizeof(*out));
	CHECK(merge_sources(&out, 4, inputs, 2, &p.base) < 0 && da_len(out) == 0);
	da_free(out);
	playlist_free(inputs[0]);
}

int main(void)
{
	const char *a[] = {"X", "x", "Y", "Z"}, *b[] = {"x", "Y", "W", "w"};
	playlist inputs[] = {test_playlist(a, 4), test_playlist(b, 4), NULL};
	all_algorithms(inputs, 3, 4);
	double weights[] = {1, 1, 1};
	playlist out = run(round_robin_merge_sources, SAMPLE_HEAD, inputs, 3, 4, weights, 4);
	const char *order[] = {"X", "Y", "Z", "W"};
	for (size_t i = 0; i < 4; i++)
		CHECK(strcmp((const char *)out[i].tr->title, order[i]) == 0);
	da_free(out);
	/* Reservoir RNG consumption depends on unique songs, not occurrences. */
	random_calls = 0;
	out = run(reservoir_merge_sources, SAMPLE_HEAD, inputs, 3, 1, weights, 1);
	CHECK(random_calls == 3);
	da_free(out);
	playlist_free(inputs[0]);
	playlist_free(inputs[1]);

	const char *same[] = {"X", "x", "Ｘ"};
	inputs[0] = test_playlist(same, 3);
	inputs[1] = test_playlist(same, 3);
	all_algorithms(inputs, 3, 1);
	playlist_free(inputs[0]);
	playlist_free(inputs[1]);
	inputs[0] = inputs[1] = NULL;
	all_algorithms(inputs, 3, 0);
	all_algorithms(NULL, 0, 0);

	double weighted[] = {2, 1, 1};
	size_t five_two_two[] = {5, 2, 2}, three_three_two[] = {3, 3, 2};
	quota_counts(weighted_merge_sources, 9, weighted, five_two_two);
	quota_counts(proportional_merge_sources, 9, weighted, five_two_two);
	quota_counts(uniform_merge_sources, 8, weighted, three_three_two);
	boundaries();

	/* Exhaust all overlaps of two sources over a three-song universe. */
	const char *universe[] = {"A", "B", "C"};
	for (unsigned ma = 0; ma < 8; ma++) {
		for (unsigned mb = 0; mb < 8; mb++) {
			const char *ta[3], *tb[3];
			size_t na = 0, nb = 0, unique = 0;
			for (unsigned k = 0; k < 3; k++) {
				if (ma & (1u << k))
					ta[na++] = universe[k];
				if (mb & (1u << k))
					tb[nb++] = universe[k];
				unique += ((ma | mb) & (1u << k)) != 0;
			}
			inputs[0] = test_playlist(ta, na);
			inputs[1] = test_playlist(tb, nb);
			all_algorithms(inputs, 2, unique);
			playlist_free(inputs[0]);
			playlist_free(inputs[1]);
		}
	}
	puts("merge-test passed");
	return 0;
}
