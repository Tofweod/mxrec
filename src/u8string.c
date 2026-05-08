#include "u8string.h"
#include "assert.h"
#include "comm.h"
#include "utf8proc/utf8proc.h"
#include "xmalloc.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

extern u8s_norm_t default_norm_type;
u8s_norm_t default_norm_type = U8S_NFKC;

// u8shdr
struct packed u8shdr {
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

static inline size_t u8savail(const u8s s)
{
	struct u8shdr *h = U8S_HDR(s);
	return h->alloc - h->len;
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

	if (unlikely(buf == NULL)) {
		ret = NULL;
		goto cleanup;
	}

	ret = u8snewplacement(buf, bufsize, s, reallen, default_norm_type);

cleanup:
	utf8proc_free(s);
	return ret;
}

/**
 * cmp between u8s on raw data that have the same normaliziton standard.
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
			panic("Normalizing u8s failed:%s", s2);
	}

	cmp = _u8scmp(s1, l2, ns, ret);

	utf8proc_free(ns);

	return cmp;
}

static inline u8s _u8sExpand(u8s s, size_t addlen)
{
	void *h, *newh;
	size_t avail = u8savail(s);
	size_t len, newlen;
	int hdrlen = U8S_HDRSIZE;

	h = U8S_HDR(s);

	if (avail >= addlen)
		return s;

	len = u8sblen(s);
	newlen = (len + addlen);
	assert(newlen > len);

	newh = xrealloc(h, hdrlen + newlen + 1);
	if (unlikely(newh == NULL))
		return NULL;
	s = (u8s)newh + hdrlen;
	u8s_set(s, alloc, newlen);
	return s;
}

// exposed api
void u8ssetblen(u8s s, size_t len)
{
	assert(u8sblen(s) >= len);
	u8s_set(s, len, len);
}

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

u8s u8sexpandzero(u8s s, size_t len)
{
	u8s_size_t curlen = u8slen(s);

	if (len <= curlen)
		return s;
	s = _u8sExpand(s, len - curlen);
	if (s == NULL)
		return NULL;

	memset(s + curlen, 0, (len - curlen + 1));
	u8s_set(s, len, len);
	return s;
}

u8s u8scatlen(u8s s, const void *t, size_t len)
{
	size_t curlen = u8sblen(s);
	s = _u8sExpand(s, len);
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
	size_t alloc = u8s_get(s, alloc);
	if (alloc < len) {
		s = _u8sExpand(s, len - u8sblen(s));
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

u8s u8scatvprintf(u8s s, const char *fmt, va_list ap)
{
	va_list cp;
	char staticbuf[1024], *buf = staticbuf;
	u8s t;
	size_t buflen = strlen(fmt) * 2;
	int bufstrlen;

	if (buflen > sizeof(staticbuf)) {
		buf = xmalloc(buflen);
		if (buf == NULL)
			return NULL;
	} else {
		buflen = sizeof(staticbuf);
	}

	while (1) {
		va_copy(cp, ap);
		// do not use
		bufstrlen = vsnprintf(buf, buflen, fmt, cp);
		va_end(cp);
		if (bufstrlen < 0) {
			if (buf != staticbuf)
				xfree(buf);
			return NULL;
		}
		if ((size_t)bufstrlen >= buflen) {
			if (buf != staticbuf)
				xfree(buf);
			buflen = ((size_t)bufstrlen) + 1;
			buf = xmalloc(buflen);
			if (buf == NULL)
				return NULL;
			continue;
		}
		break;
	}

	t = u8scatlen(s, buf, bufstrlen);
	if (buf != staticbuf)
		xfree(buf);
	return t;
}

u8s u8scatprintf(u8s s, const char *fmt, ...)
{
	va_list ap;
	u8s t;
	va_start(ap, fmt);
	t = u8scatvprintf(s, fmt, ap);
	va_end(ap);
	return t;
}

// TODO

void u8sclear(u8s s)
{
	u8ssetblen(s, 0);
	s[0] = '\0';
}

int u8scmp(const u8s s1, const u8s s2)
{
	int cmp;
	struct u8shdr *h1, *h2;
	u8s ns1, ns2;
	bool n1 = false, n2 = false;

	h1 = U8S_HDR(s1);
	h2 = U8S_HDR(s2);

	if (h1->valid_type && h2->valid_type) {
		return _u8scmpNorm(s1, s2);
	}
	if (h1->valid_type == 0) {
		ns1 = u8s_proc(s1, u8sblen(s1),
			       u8snormtype2option(default_norm_type));
		n1 = true;
		if (ns1 == NULL)
			goto err;
	} else {
		ns1 = s1;
	}
	if (h2->valid_type == 0) {
		ns2 = u8s_proc(s2, u8sblen(s2),
			       u8snormtype2option(default_norm_type));
		n2 = true;
		if (ns1 == NULL)
			goto err;
	} else {
		ns2 = s2;
	}

	cmp = _u8scmp(ns1, u8sblen(ns1), ns2, u8sblen(ns2));

	if (n1)
		u8sfree(ns1);
	if (n2)
		u8sfree(ns2);

	return cmp;

err:
	if (n1 && ns1)
		u8sfree(ns1);
	// defensive code
	if (n2 && ns2)
		u8sfree(ns2);
	panic("Normalizing u8s failed.");
}

u8s u8sjoin(char **argv, int argc, char *sep)
{
	u8s join = u8sempty();
	int j;

	for (j = 0; j < argc; ++j) {
		join = u8scat(join, argv[j]);
		if (j != argc - 1)
			join = u8scat(join, sep);
	}

	return join;
}

u8s u8sjoinu8s(u8s *argv, int argc, const char *sep, size_t seplen)
{
	u8s join = u8sempty();

	int j;

	for (j = 0; j < argc; ++j) {
		join = u8scatu8s(join, argv[j]);
		if (j != argc - 1)
			join = u8scatlen(join, sep, seplen);
	}
	return join;
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

	if (unlikely(buf == NULL)) {
		ret = NULL;
		goto cleanup;
	}

	ret = u8snewplacement(buf, bufsize, dst, dstlen, u8soption2normtype(opts));

cleanup:
	utf8proc_free(dst);
	return ret;
}
