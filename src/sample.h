#ifndef TOF_MXREC_SAMPLE_H
#define TOF_MXREC_SAMPLE_H

#include <stdbool.h>
#include <stddef.h>

enum sample_method { SAMPLE_HEAD, SAMPLE_RANDOM };

typedef struct sample_cursor {
	size_t *order;
	size_t len;
	size_t cursor;
} sample_cursor;

/* Iterates without replacement; never rearranges source items. */
int sample_cursor_init(sample_cursor *state, size_t len, enum sample_method method);
bool sample_cursor_next(sample_cursor *state, size_t *index);
void sample_cursor_free(sample_cursor *state);

#endif
