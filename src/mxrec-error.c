#include "mxrec-error.h"
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

void mxrec_error_clear(mxrec_error *err)
{
	if (err)
		memset(err, 0, sizeof(*err));
}
void mxrec_error_add_at(mxrec_error *err, const char *file, int line, const char *function, const char *step,
			const char *fmt, ...)
{
	if (!err || !fmt)
		return;
	if (err->count == MXREC_ERROR_MAX_FRAMES) {
		err->omitted++;
		return;
	}
	mxrec_error_frame *f = &err->frames[err->count++];
	f->file = file;
	f->line = line;
	f->function = function;
	int a = snprintf(f->step, sizeof(f->step), "%s", step ? step : "unknown");
	va_list ap;
	va_start(ap, fmt);
	int b = vsnprintf(f->message, sizeof(f->message), fmt, ap);
	va_end(ap);
	if (b < 0)
		snprintf(f->message, sizeof(f->message), "failed to format error");
	f->truncated = a < 0 || (size_t)a >= sizeof(f->step) || b < 0 || (size_t)b >= sizeof(f->message);
	for (char *p = f->message; *p; p++)
		if ((unsigned char)*p < 32 || *p == 127)
			*p = ' ';
}
void mxrec_error_print(FILE *out, const mxrec_error *err, const char *source, unsigned long long attempt,
		       unsigned long long max_attempts, int status, bool partial)
{
	if (!out || !err || !err->count)
		return;
	flockfile(out);
	fprintf(out, "[%s] source=%s attempt=%llu/%llu%s\n", status < 0 ? "error" : "warning",
		source ? source : "unknown", attempt, max_attempts, partial ? " (partial results retained)" : "");
	for (size_t i = 0; i < err->count; i++) {
		const mxrec_error_frame *f = &err->frames[i];
		fprintf(out, "  %s: %s%s (%s:%d, %s)\n", f->step, f->message, f->truncated ? " [truncated]" : "",
			f->file ? f->file : "?", f->line, f->function ? f->function : "?");
	}
	if (err->omitted)
		fprintf(out, "  ... %zu additional error frames omitted\n", err->omitted);
	funlockfile(out);
}
