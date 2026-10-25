#include "normalize.h"
#include "utf8proc/utf8proc.h"
#include "xmalloc.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static bool key_space(utf8proc_int32_t cp)
{
	utf8proc_category_t category = utf8proc_category(cp);
	return (cp >= 9 && cp <= 13) || cp == 0x85 || category == UTF8PROC_CATEGORY_ZS ||
	       category == UTF8PROC_CATEGORY_ZL || category == UTF8PROC_CATEGORY_ZP;
}

int normalize_utf8_key(const char *input, size_t input_len, char **output, size_t *output_len)
{
	utf8proc_uint8_t *mapped = NULL;
	utf8proc_ssize_t len;
	bool space = false;
	size_t used = 0;
	char *result;

	if (!output || !output_len)
		return -1;
	*output = NULL;
	*output_len = 0;
	if (!input || input_len > PTRDIFF_MAX)
		return -1;
	len = utf8proc_map((const utf8proc_uint8_t *)input, input_len, &mapped,
			   UTF8PROC_STABLE | UTF8PROC_COMPAT | UTF8PROC_COMPOSE | UTF8PROC_CASEFOLD);
	if (len < 0)
		return -1;
	result = xmalloc((size_t)len + 1);
	for (utf8proc_ssize_t pos = 0; pos < len;) {
		utf8proc_int32_t cp;
		utf8proc_ssize_t n = utf8proc_iterate(mapped + pos, len - pos, &cp);
		if (n <= 0) {
			xfree(result);
			utf8proc_free(mapped);
			return -1;
		}
		if (key_space(cp)) {
			space = used > 0;
		} else {
			if (space)
				result[used++] = ' ';
			memcpy(result + used, mapped + pos, (size_t)n);
			used += (size_t)n;
			space = false;
		}
		pos += n;
	}
	result[used] = '\0';
	utf8proc_free(mapped);
	*output = result;
	*output_len = used;
	return 0;
}
