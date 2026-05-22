#ifndef TOF_MXREC_LASTFM_API_H
#define TOF_MXREC_LASTFM_API_H

// methods of api
#define LASTFMAPI_FORMAT "json"
#define LASTFMAPI_USER_GETRECENTTRACKS "User.getrecenttracks"
#define LASTFMAPI_TRACK_GETSIMILAR
#define LASTFMAPI_ARTIST_GETSIMILAR

#include "comm-source.h"


typedef struct lastfmapi_source lastfmapi_source;

int lastfmapi_source_new(source **src, config_t *cfg);

#endif // !TOF_MXREC_LASTFM_API_H
