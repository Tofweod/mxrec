#include "sample.h"
#include "random.h"
#include "xmalloc.h"
#include <stdint.h>
#include <string.h>

int sample_cursor_init(sample_cursor *state, size_t len, enum sample_method method)
{
	memset(state, 0, sizeof(*state));
	if ((method != SAMPLE_HEAD && method != SAMPLE_RANDOM) || len > SIZE_MAX / sizeof(size_t))
		return -1;
	state->len = len;
	if (method == SAMPLE_HEAD || !len)
		return 0;
	state->order = xmalloc(len * sizeof(*state->order));
	for (size_t i = 0; i < len; i++)
		state->order[i] = i;
	for (size_t i = len; i > 1; i--) {
		double value;
		if (uniform01(&value) < 0) {
			sample_cursor_free(state);
			return -1;
		}
		size_t j = (size_t)(value * i);
		size_t tmp = state->order[i - 1];
		state->order[i - 1] = state->order[j];
		state->order[j] = tmp;
	}
	return 0;
}

bool sample_cursor_next(sample_cursor *state, size_t *index)
{
	if (state->cursor >= state->len)
		return false;
	*index = state->order ? state->order[state->cursor] : state->cursor;
	state->cursor++;
	return true;
}

void sample_cursor_free(sample_cursor *state)
{
	xfree(state->order);
	memset(state, 0, sizeof(*state));
}
