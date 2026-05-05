#include "u8string.h"
#include "assert.h"
#include "comm.h"
#include "utf8proc/utf8proc.h"
#include "xmalloc.h"
#include <string.h>

extern u8s_norm_t default_norm_type;
u8s_norm_t default_norm_type = U8S_NFKC;

// u8shdr
struct __attribute__((__packed__)) u8shdr {
	// byte len of u8s
	size_t len;
	// alloced bytes of current string
	size_t alloc;
	// normalize type
	u8s_norm_t type : 4;
	bool valid_type : 1;
	u8s buf[];
};

#define U8S_HDR(s) ((struct u8shdr *)((s) - (sizeof(struct u8shdr))))
#define U8S_HDRSIZE (sizeof(struct u8shdr))

u8s_size_t u8slen(const u8s s)
{
	size_t bn;
	u8s_ssize_t len;
	u8s _s = s;
	u8s_size_t n = 0;
	u8cp c;

	bn = u8sblen(s);

	while (bn) {
		len = utf8proc_iterate(_s, -1, &c);
		if (c == -1) {
			++_s;
			--bn;
			continue;
		}
		_s += len;
		bn -= len;
		++n;
	}
	return n;
}

size_t u8sblen(const u8s s)
{
	return U8S_HDR(s)->len;
}

// static auxiliary functions
#define u8s_set(s, field, val) (U8S_HDR((s))->field = (val))
#define u8s_get(s, field) (U8S_HDR((s))->field)

static inline u8s_norm_t u8soption2normtype(u8s_option_t type)
{
	if (type & (U8S_COMPOSE | U8S_COMPAT)) {
		return U8S_NFKC;
	} else if (type & (U8S_DECOMPOSE | U8S_COMPAT)) {
		return U8S_NFKD;
	} else if (type & U8S_COMPOSE) {
		return U8S_NFC;
	} else if (type & U8S_DECOMPOSE) {
		return U8S_NFD;
	} else {
		panic("Cannot extract normalization type from options");
	}
	mxrec_unreachable();
}

static inline u8s_option_t u8snormtype2option(u8s_norm_t type)
{
	switch (type) {
	case U8S_NFC:
		return U8S_COMPOSE;
	case U8S_NFD:
		return U8S_DECOMPOSE;
	case U8S_NFKC:
		return U8S_COMPOSE | U8S_COMPAT;
	case U8S_NFKD:
		return U8S_DECOMPOSE | U8S_COMPAT;
	default:
		panic("Unknown u8s normaliziton type");
	}
	mxrec_unreachable();
}

static inline u8s _u8snewlen(const void *init, size_t len)
{
	u8s s, buf, ret;
	size_t bufsize;
	u8s_ssize_t reallen;
	int hdrlen = U8S_HDRSIZE;

	assert(len + hdrlen + 1 > len);

	// normalize
	reallen = utf8proc_map(init, len, &s,
			       (utf8proc_option_t)u8snormtype2option(default_norm_type));

	if (reallen < 0) {
		ret = NULL;
		goto cleanup;
	}

	bufsize = reallen + hdrlen + 1;
	assert(bufsize > reallen);
	buf = xmalloc(bufsize);

	if (buf == NULL) {
		ret = NULL;
		goto cleanup;
	}

	ret = u8snewplacement(buf, bufsize, s, reallen, default_norm_type);

cleanup:
	utf8proc_free(s);
	return ret;
}

/**
 * cmp between u8s on raw data
 */
static inline int _u8scmp(const u8s s1, size_t l1, const u8s s2, size_t l2)
{
	size_t minlen;
	int cmp;
	minlen = (l1 > l2) ? l1 : l2;
	cmp = memcmp(s1, s2, minlen);
	if (cmp == 0)
		return l1 > l2 ? 1 : (l1 < l2 ? -1 : 0);
	return cmp;
}

/*
 * cmp between two u8s both of that are normalized.
 */
static inline int _u8scmpNorm(const u8s s1, const u8s s2)
{
	struct u8shdr *h1, *h2;
	size_t l1, l2;
	u8s ns;
	u8s_ssize_t ret;
	int cmp;

	h1 = U8S_HDR(s1);
	h2 = U8S_HDR(s2);

	l1 = h1->len;
	l2 = h2->len;

	if (h1->type == h2->type) {
		return _u8scmp(s1, l1, s2, l2);
	}
	// convert s2 into s1' normaliziton
	else {
		ret = utf8proc_map(s2, l2, &ns,
				   (utf8proc_option_t)u8snormtype2option(h1->type));
		if (ret < 0)
			goto err;
	}

	cmp = _u8scmp(s1, l2, ns, ret);

	utf8proc_free(ns);

	return cmp;
err:
	panic("Normalizing u8s failed:%s", s2);
}

// Make room with assigned character length of 'len', record really used bytes in 'used'
// Return new allocated u8s, the old u8s is deprecated.
// We simply assume a codepoint's byte length is U8S_AVG_CHAR_SIZE
static inline u8s _u8sExpand(u8s s, u8s_size_t len, size_t *used)
{
	// TODO
}

static inline u8s _u8sExpandblen(u8s s, size_t len)
{
	// TODO
}

// exposed api
u8s u8snewlen(const void *init, size_t len)
{
	return _u8snewlen(init, len);
}

u8s u8snew(const char *init)
{
	size_t len = (init == NULL) ? 0 : strlen(init);
	return u8snewlen(init, len);
}

u8s u8snewplacement(void *buf, size_t bufsize, const u8s init, size_t len, u8s_norm_t type)
{
	assert(bufsize >= len + U8S_HDRSIZE + 1);
	int hdrlen = U8S_HDRSIZE;
	u8s s = (u8s)buf + hdrlen;
	size_t usable = bufsize - hdrlen - 1;

	// hdr settings
	u8s_set(s, len, len);
	u8s_set(s, alloc, usable);
	u8s_set(s, type, type);
	u8s_set(s, valid_type, 1);

	if (!init)
		memset(s, 0, len);
	else if (len)
		memcpy(s, init, len);

	s[len] = '\0';
	return s;
}

u8s u8sempty(void)
{
	return u8snewlen("", 0);
}

u8s u8sdup(const u8s s)
{
	return u8snewlen(s, u8sblen(s));
}

void u8sfree(u8s s)
{
	if (s == NULL)
		return;
	xfree(U8S_HDR(s));
}

u8s u8sexpandzero(u8s s, u8s_size_t len)
{
	size_t used;
	u8s_size_t curlen = u8slen(s);

	if (len <= curlen)
		return s;
	s = _u8sExpand(s, len - curlen, &used);
	if (s == NULL)
		return NULL;

	memset(s + curlen, 0, (used - curlen + 1));
	u8s_set(s, len, used);
	return s;
}

u8s u8scatlen(u8s s, const void *t, size_t len)
{
	size_t curlen = u8sblen(s);
	s = _u8sExpandblen(s, len);
	if (s == NULL)
		return NULL;
	memcpy(s + curlen, t, len);
	s[curlen + len] = '\0';
	u8s_set(s, len, curlen + len);
	u8s_set(s, valid_type, 0);
	return s;
}

u8s u8scpylen(u8s s, const void *t, size_t len)
{
	// TODO
	size_t alloc = u8s_get(s, alloc);
	if (alloc < len) {
		s = _u8sExpandblen(s, len - u8sblen(s));
	}
	memcpy(s, t, len);
	s[len] = '\0';
	u8s_set(s, len, len);
	u8s_set(s, valid_type, 0);
	return s;
}

void u8snormalize(u8s *s, u8s_norm_t type)
{

	u8s new_s, old_s = *s;
	new_s = u8s_proc(old_s, u8sblen(old_s),
			 u8snormtype2option(type));
	u8sfree(old_s);
	*s = new_s;
	u8s_set(*s, valid_type, 1);
}

u8cp u8cpdecode(void *cp)
{
	u8cp ret;
	utf8proc_iterate(cp, -1, &ret);
	return ret;
}

u8s u8schr(const u8s s, u8cp c)
{
	size_t bn;
	u8s _s = s;
	u8cp _c;
	u8s_ssize_t len;

	if (c == -1)
		return NULL;

	bn = u8sblen(s);

	while (bn) {
		len = utf8proc_iterate(_s, -1, &_c);
		if (_c == -1) {
			++_s;
			--bn;
			continue;
		}
		if (_c == c) {
			return _s;
		}
		_s += len;
		bn -= len;
	}

	return NULL;
}

u8cp *u8s2codepoint(const u8s s, size_t *len)
{
	size_t bn;
	u8s _s;
	u8cp c, *cps;
	u8s_size_t _len, n;

	_s = s;
	n = 0;
	bn = u8sblen(s);

	_len = u8slen(_s);
	cps = xmalloc(_len);

	while (bn) {
		u8s_ssize_t cl = utf8proc_iterate(_s, -1, &c);
		if (c == -1) {
			++_s;
			--bn;
			continue;
		}
		_s += cl;
		bn -= cl;
		cps[n++] = c;
	}

	if (len)
		*len = n;
	return cps;
}

void u8strim(u8s s, const u8s cset)
{
	// TODO
}

u8s u8scat(u8s s, const char *t)
{
	return u8scatlen(s, t, strlen(t));
}

u8s u8scatu8s(u8s s, const u8s t)
{
	return u8scatlen(s, t, u8sblen(t));
}

u8s u8scpy(u8s s, const char *t)
{
	return u8scpylen(s, t, strlen(t));
}

u8s u8scpyu8s(u8s s, const u8s t)
{
	return u8scpylen(s, t, u8sblen(t));
}

int u8scmp(const u8s s1, const u8s s2)
{
	struct u8shdr *h1, *h2;

	h1 = U8S_HDR(s1);
	h2 = U8S_HDR(s2);

	if (h1->valid_type && h2->valid_type) {
		return _u8scmpNorm(s1, s2);
	}
	// TODO
}

u8s u8s_proc(const u8s src, u8s_ssize_t srclen, u8s_option_t opts)
{
	u8s dst, buf, ret;
	size_t bufsize;
	u8s_ssize_t dstlen;

	dstlen = utf8proc_map(src, srclen, &dst, (utf8proc_option_t)opts);

	if (dstlen < 0) {
		ret = NULL;
		goto cleanup;
	}

	bufsize = dstlen + U8S_HDRSIZE + 1;

	buf = xmalloc(bufsize);

	if (buf == NULL) {
		ret = NULL;
		goto cleanup;
	}

	ret = u8snewplacement(buf, bufsize, dst, dstlen, u8soption2normtype(opts));

cleanup:
	utf8proc_free(dst);
	return ret;
}
