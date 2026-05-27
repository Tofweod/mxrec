#include "json.h"
#include <stdarg.h>
#include <stdio.h>

void write_jsonerr(void *err, const char *fmt, ...)
{
	struct json_err *error = (struct json_err *)err;
	va_list args;
	va_start(args, fmt);
	error->length += vsnprintf((char *)error->msg + error->length, MAX_JSON_ERR_LEN - error->length, fmt, args);
	va_end(args);
}

void print_jsonerr(void *err)
{
	struct json_err *error = (struct json_err *)err;
	if (error->length == 0)
		return;
	fprintf(stderr, "Error in parsing json: %s", error->msg);
}
