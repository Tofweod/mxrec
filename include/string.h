#ifndef TOF_STRING_H
#define TOF_STRING_H

#include "comm.h"

// utf-8 string
typedef u8_t *u8s;
typedef utf8proc_size_t u8s_size_t;
typedef utf8proc_property_t u8s_property;
typedef enum {
#define U8S_OPTION_LIST       \
	U8S_OPTION(NULLTERM)  \
	U8S_OPTION(STABLE)    \
	U8S_OPTION(COMPAT)    \
	U8S_OPTION(COMPOSE)   \
	U8S_OPTION(DECOMPOSE) \
	U8S_OPTION(IGNORE)    \
	U8S_OPTION(REJECTNA)  \
	U8S_OPTION(NLF2LS)    \
	U8S_OPTION(NLF2PS)    \
	U8S_OPTION(NLF2LF)    \
	U8S_OPTION(STRIPCC)   \
	U8S_OPTION(CASEFOLD)  \
	U8S_OPTION(CHARBOUND) \
	U8S_OPTION(LUMP)      \
	U8S_OPTION(STRIPMARK) \
	U8S_OPTION(STRIPNA)

#define U8S_OPTION(type) U8S_##type = UTF8PROC_##type,
	U8S_OPTION_LIST
} u8s_option_t;

struct u8shdr {
	u8s_size_t len;
	u8s buf[];
};

#define U8S_HDR(s) ((struct u8shdr *)((s) - (sizeof(struct u8shdr))))

static inline u8s_size_t u8slen(const u8s s)
{
	return U8S_HDR(s)->len;
}

u8s u8snewlen(const void *init, u8s_size_t len);
u8s u8snew(const void *init);
u8s u8snewplacement(char *buf, size_t bufsize, const char *init, u8s_size_t len);

u8s u8sempty(void);
u8s u8sdup(const u8s s);
void u8sfree(u8s s);
u8s u8sexpandzero(u8s, u8s_size_t len);
u8s u8scatlen(u8s s, const void *t, u8s_size_t len);
u8s u8scat(u8s s, const char *t);
u8s u8scatu8s(u8s s, const u8s t);
u8s u8scpylen(u8s s, const char *t, u8s_size_t len);
u8s u8scpy(u8s s, const char *t);

void u8strim(u8s s, const char *cset);
// TODO

int u8scmp(const u8s s1, const u8s s2);
u8s u8scatrepr(u8s s, const char *p, u8s_size_t len);
u8s u8sjoin(char **argv, int argc, char *sep);
u8s u8sjoinu8s(u8s *argv, int argc, const char *sep, u8s_size_t seplen);
int u8sneedsrepr(const u8s s);

u8s u8s_proc(const u8s src, u8s_size_t srclen, u8s_option_t opts);

#endif // !TOF_STRING_H
