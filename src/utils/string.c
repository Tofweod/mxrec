#include "utils/string.h"
#include "bb.h"
#include "comm.h"
#include "xmalloc.h"

#define DEFAULT_FORMAT_CAP 8

#define ISKVEND(kv) ((kv)->key == NULL)

BUFFERBUILDER_INIT(static, StrBuffer, _sb, char);

static int sb_init(StrBuffer *sb, size_t cap)
{
	if (_sb_init(sb, cap) < 0)
		return -1;
	sb->buf[0] = '\0';
	return 0;
}

mxrec_unused static void sb_free(StrBuffer *sb)
{
	_sb_free(sb);
}

mxrec_unused static void sb_clear(StrBuffer *sb)
{
	_sb_clear(sb);
}

static int sb_append_str(StrBuffer *sb, const char *s)
{
	size_t n = strlen(s);
	if (_sb_append(sb, s, n + 1) < 0)
		return -1;
	// null terminal
	sb->buf[--(sb->len)] = '\0';
	return 0;
}

static int sb_append_char(StrBuffer *sb, int ch)
{
	if (_sb_append(sb, &ch, 1) < 0)
		return -1;
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

	if (sb_init(&sb, strlen(fmt)) < 0)
		mxrec_cleanup(cleanup, ret, 0);

	if (__parseKVFormat(fmt, pairs, &sb) < 0)
		mxrec_cleanup(cleanup, ret, 0);

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
