#include "utils/string.h"
#include "xmalloc.h"

#define DEFAULT_FORMAT_CAP 8

#define ISKVEND(kv) ((kv)->key == NULL)

typedef struct {
	char *buf;
	size_t len;
	size_t cap;
} StrBuffer;

static int sb_init(StrBuffer *sb, size_t cap)
{
	sb->cap = cap;
	sb->len = 0;
	sb->buf = xmalloc(sb->cap);
	if (sb->buf == NULL) {
		memset(sb, 0, sizeof(*sb));
		return -1;
	}
	sb->buf[0] = '\0';
	return 0;
}

static int sb_reserve(StrBuffer *sb, size_t need)
{
	if (sb->len + need + 1 <= sb->cap)
		return 0;

	size_t old_cap = sb->cap;
	assert(sb->len + need + 1 > sb->len);
	while (sb->len + need + 1 > sb->cap)
		sb->cap *= 2;

	assert(old_cap <= sb->cap);

	sb->buf = xrealloc(sb->buf, sb->cap);

	if (sb->buf == NULL)
		return -1;

	return 0;
}

static int sb_append_str(StrBuffer *sb, const char *s)
{
	size_t n = strlen(s);
	if (sb_reserve(sb, n) < 0)
		return -1;

	memcpy(sb->buf + sb->len, s, n);

	sb->len += n;
	sb->buf[sb->len] = '\0';
	return 0;
}

static int sb_append_char(StrBuffer *sb, int ch)
{
	if (sb_reserve(sb, 1) < 0)
		return -1;

	sb->buf[sb->len++] = ch;
	sb->buf[sb->len] = '\0';
	return 0;
}

static const char *kvgetkey(kv_t *kvs, const char *key)
{
	kv_t *cur = kvs;
	while (!ISKVEND(cur)) {
		if (strcmp(cur->key, key) == 0) {
			return cur->val;
		}
		++cur;
	}
	return NULL;
}

static int __parseKVFormat(const char *fmt, kv_t *kvs, StrBuffer *sb)
{
	const char *p = fmt;
	char key[MAX_KEY_SIZE + 1];
	while (*p) {
		if (*p == '{') {
			const char *start = ++p;

			while (*p && *p != '}')
				++p;

			size_t len = p - start;

			strncpy(key, start, len);
			key[len] = '\0';

			const char *val = kvgetkey(kvs, key);
			if (val)
				if (sb_append_str(sb, val) < 0)
					return -1;

			if (*p == '}')
				++p;
		} else {
			if (sb_append_char(sb, *p) < 0)
				return -1;
			p++;
		}
	}
	return 0;
}

char *parseKVFormat(const char *fmt, ...)
{
	va_list args;
	kv_t *pairs;
	size_t n = 0, cap = DEFAULT_FORMAT_CAP;
	StrBuffer sb;
	char *ret;

	va_start(args, fmt);

	pairs = xmalloc(sizeof(kv_t) * cap);
	if (pairs == NULL)
		return NULL;

	while (1) {
		kv_t kv = va_arg(args, kv_t);

		if (ISKVEND(&kv))
			break;

		if (n >= cap) {
			cap *= 2;

			pairs = xrealloc(pairs, sizeof(kv_t) * cap);
			if (pairs == NULL)
				return NULL;
		}

		pairs[n++] = kv;
	}

	va_end(args);

	pairs[n] = KV_END;

	if (sb_init(&sb, strlen(fmt)) < 0) {
		ret = NULL;
		goto cleanup;
	}

	if (__parseKVFormat(fmt, pairs, &sb) < 0) {
		ret = NULL;
		goto cleanup;
	}

	ret = sb.buf;
cleanup:
	xfree(pairs);

	return ret;
}

char *parseFormat(const char *fmt, ...)
{
	char *result;
	va_list ap;
	va_start(ap, fmt);
	result = parsevFormat(fmt, ap);
	va_end(ap);
	return result;
}

char *parsevFormat(const char *fmt, va_list ap)
{
	char *s;
	xvasprintf(&s, fmt, ap);
	return s;
}
