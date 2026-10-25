#ifndef TOF_MXREC_NORMALIZE_H
#define TOF_MXREC_NORMALIZE_H

#include <stddef.h>

/* NFKC + case folding + Unicode whitespace trimming/collapsing.
 * The caller owns *output (xfree). Returns -1 for invalid UTF-8/input. */
int normalize_utf8_key(const char *input, size_t input_len, char **output, size_t *output_len);

#endif
