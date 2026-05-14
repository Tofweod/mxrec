#ifndef TOF_MXREC_CURLIMPERSONATE_H
#define TOF_MXREC_CURLIMPERSONATE_H

#include <curl/curl.h>
#ifndef WITHOUT_CURL_IMPERSONATE
CURL_EXTERN CURLcode curl_easy_impersonate(CURL *curl, const char *target,
					   int default_headers);
#else
#define curl_easy_impersonate(...) (__VA_ARGS__)
#endif

#endif
