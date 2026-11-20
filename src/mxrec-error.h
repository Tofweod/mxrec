#ifndef MXREC_ERROR_H
#define MXREC_ERROR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#define MXREC_ERROR_MAX_FRAMES 32
#define MXREC_ERROR_STEP_SIZE 64
#define MXREC_ERROR_MESSAGE_SIZE 256
#if defined(__GNUC__) || defined(__clang__)
#define MXREC_PRINTF_FORMAT(a, b) __attribute__((format(printf, a, b)))
#else
#define MXREC_PRINTF_FORMAT(a, b)
#endif
typedef struct mxrec_error_frame {
	const char *file, *function;
	int line;
	char step[MXREC_ERROR_STEP_SIZE];
	char message[MXREC_ERROR_MESSAGE_SIZE];
	bool truncated;
} mxrec_error_frame;
typedef struct mxrec_error {
	mxrec_error_frame frames[MXREC_ERROR_MAX_FRAMES];
	size_t count, omitted;
} mxrec_error;
void mxrec_error_clear(mxrec_error *err);
void mxrec_error_add_at(mxrec_error *err, const char *file, int line, const char *function, const char *step,
			const char *fmt, ...) MXREC_PRINTF_FORMAT(6, 7);
void mxrec_error_print(FILE *out, const mxrec_error *err, const char *source, unsigned long long attempt,
		       unsigned long long max_attempts, int status, bool partial);
#define mxrec_error_add(err, step, ...) mxrec_error_add_at((err), __FILE__, __LINE__, __func__, (step), __VA_ARGS__)
#endif
