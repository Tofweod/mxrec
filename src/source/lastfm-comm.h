#ifndef TOF_MXREC_LASTFM_COMM_H
#define TOF_MXREC_LASTFM_COMM_H

typedef struct source source;
typedef struct config_t config_t;

typedef struct lastfm_security {
	char *profile;
} lastfm_security;

extern int lastfm_security_init(config_t *cfg);
extern void lastfm_security_free();
extern lastfm_security __lastfm_security;

#endif
