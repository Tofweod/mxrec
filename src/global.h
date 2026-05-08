#ifndef TOF_MXREC_MONOTONIC_H
#define TOF_MXREC_MONOTONIC_H

typedef struct config_t config_t;

void globalCurlInit(void);

void globalCurlCleanup(void);

void loadGlobalConfig(const char *filename);

config_t *getGlobalConfig(void);

void globalConfigCleanup(void);

void globalCleanup(void);

#endif
