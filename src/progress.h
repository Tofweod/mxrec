#ifndef TOF_MXREC_PROGRESS_H
#define TOF_MXREC_PROGRESS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct progress   progress;
typedef struct prog_bar   prog_bar;

progress *progress_new(bool threaded);
void      progress_free(progress *p);
prog_bar *progress_bar_add(progress *p, const char *name, size_t total);
void      progress_bar_update(prog_bar *b, size_t done, size_t total, const char *desc);
void      progress_bar_tick(prog_bar *b, size_t total, const char *desc);

void progress_entry_update(void *entry, size_t cur, size_t total, const char *desc);
void progress_entry_clear(void *entry);

#ifdef __cplusplus
}
#endif

#endif
