#include "utils/file.h"
#include "assert.h"
#include "xmalloc.h"
#include <stdio.h>
#include <string.h>

char *slurp(const char *path)
{
	FILE *fp;
	long sz;
	char *buf;
	char *start;
	char *end;

	fp = fopen(path, "r");
	if (!fp) {
		error("cannot open file: %s", path);
		return NULL;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		error("cannot seek file: %s", path);
		fclose(fp);
		return NULL;
	}
	sz = ftell(fp);
	if (sz <= 0) {
		error("empty or unreadable file: %s", path);
		fclose(fp);
		return NULL;
	}
	rewind(fp);

	buf = xmalloc(sz + 1);
	if (fread(buf, 1, sz, fp) != (size_t)sz) {
		error("failed to read file: %s", path);
		fclose(fp);
		xfree(buf);
		return NULL;
	}
	buf[sz] = '\0';
	fclose(fp);

	/* strip leading whitespace */
	start = buf;
	while (start < buf + sz && (*start == '\n' || *start == '\r' || *start == ' ' || *start == '\t'))
		start++;

	/* strip trailing whitespace */
	end = buf + sz - 1;
	while (end > start && (*end == '\n' || *end == '\r' || *end == ' ' || *start == '\t'))
		end--;
	*(end + 1) = '\0';

	if (start > end) {
		buf[0] = '\0';
		return buf;
	}

	if (start != buf)
		memmove(buf, start, end - start + 2);

	return buf;
}
