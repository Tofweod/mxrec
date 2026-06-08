#ifndef TOF_MXREC_UTIL_IMAGE_H
#define TOF_MXREC_UTIL_IMAGE_H

#include <stddef.h>

// get raw data of base64, need to release the returned pointe through xfree
char *b64decode(const char *b64rawdata, size_t *outlen);

// in-place pointer of 'b64data'
const char *b64rawdata(const char *b64data);

int b64writeQR(const char *b64data, const char *outfile, int margin, int use_ansi, int invert, int resize);

int strwriteQR(const char *strdata, const char *outfile, int margin, int use_ansi, int invert, int resize);

#endif // !TOF_MXREC_UTIL_IMAGE_H
