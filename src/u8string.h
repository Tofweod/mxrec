#ifndef TOF_MXREC_U8STRING_H
#define TOF_MXREC_U8STRING_H

/**
 * utf8 string, which is the wrapper of utf8proc library.
 * utf8 string is stored in u8 char while a single character is consists of multi u8 chars.
 * the character of utf8 string corresponds to `codepoint`, a int32 number
 * encoding a single character.
 * Thus, utf8 string has two lenghts:
 * - storage size of byte length(u8sblen)
 *   We use typical size_t to represent byte length, as it is just u8 char
 *   stream. The specifiaction guarentees that utf8 string is end of '\0'.
 * - character length(u8slen)
 *   u8s_size_t is character length, which will be used in following api.
 *   **NOTE: this is Unicode character length, at the level of code point,
 *   NOT grapheme cluster that is at the visible character level.**
 *
 * All utf8 string is initialized with normalization. The normalization standards
 * are as below:
 * - NFC
 * - NFD
 * - NFKD
 * - NFKC(default)
 * `u8snormtype` is used for changing the default normalize standard.
 * **NOTE: this library don NOT guarentee that initialized string will change
 * its norm type with `u8snormtype` function.**
 * utf8 string supports other operations on the whole string as defined in ENUM
 * u8s_option_t, with `u8s_proc` function to operate.
 */

// utf-8 string
#include "utf8proc/utf8proc.h"

// wrapper of utf8proc string
typedef utf8proc_uint8_t *u8s;

// char len
typedef utf8proc_size_t u8s_size_t;

// copdepoint
typedef utf8proc_int32_t u8cp;

typedef utf8proc_ssize_t u8s_ssize_t;

#define U8S_AVG_CHAR_SIZE 2

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
#undef U8S_OPTION
} u8s_option_t;

typedef enum {
	U8S_NFC,
	U8S_NFD,
	U8S_NFKC,
	U8S_NFKD,
} u8s_norm_t;

extern u8s_norm_t default_norm_type;

static inline void u8snormtype(u8s_norm_t type)
{
	default_norm_type = type;
}

// character length, slowly
u8s_size_t u8slen(const u8s s);
// byte length
size_t u8sblen(const u8s s);
u8s u8snewlen(const void *init, size_t len);
u8s u8snew(const char *init);
u8s u8snewplacement(void *buf, size_t bufsize, const u8s init, size_t len, u8s_norm_t type);

u8s u8sempty(void);
u8s u8sdup(const u8s s);
void u8sfree(u8s s);
u8s u8sexpandzero(u8s s, u8s_size_t len);

/**
 * u8s operated such as cat, cpy will simply use memory operations in stdlib.h
 * And it just change raw data without normaliziton.
 * Thus the type field of is no longer valid and the valid_type filed is set to 0,
 * until explicit calling `u8snormalize`.
 */
// cat with binary-safe string pointed by 't' of 'len' bytes
u8s u8scatlen(u8s s, const void *t, size_t len);
// cat with null-terminated C char
u8s u8scat(u8s s, const char *t);
// cat with u8s
u8s u8scatu8s(u8s s, const u8s t);

u8s u8scpylen(u8s s, const void *t, size_t len);
u8s u8scpy(u8s s, const char *t);
u8s u8scpyu8s(u8s s, const u8s t);

void u8snormalize(u8s *s, u8s_norm_t type);

// return -1 on error
u8cp u8cpdecode(void *cp);
u8s u8schr(const u8s s, u8cp c);

// record length of returned codepoint array
u8cp *u8s2codepoint(const u8s s, size_t *len);
void u8strim(u8s s, const u8s cset);
// TODO

/**
 * u8scmp
 * If s1 and s2 are the same after normaliziton. we regarded that they are the
 * same. Use default_norm_type to normalize.
 */
int u8scmp(const u8s s1, const u8s s2);
u8s u8scatrepr(u8s s, const char *p, u8s_size_t len);
u8s u8sjoin(char **argv, int argc, char *sep);
u8s u8sjoinu8s(u8s *argv, int argc, const char *sep, u8s_size_t seplen);
int u8sneedsrepr(const u8s s);

/**
 * u8s_proc
 * Return a new u8s operated by opts, src maintains unchanged.
 *
 * The implement of this function is based on
 * utf8proc_ssize_t utf8proc_map( const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_uint8_t **dstptr, utf8proc_option_t options );
 *
 * I am not sure what will happen if strlen is smaller than strlen(str) without
 * UTF8PROC_NULLTERM flag, so here `srclen` is simply passed into `strlen` of utf8proc_map
 */
u8s u8s_proc(const u8s src, u8s_ssize_t srclen, u8s_option_t opts);

#endif // !TOF_MXREC_U8STRING_H
