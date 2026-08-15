#ifndef TOF_MXREC_DUMP_H
#define TOF_MXREC_DUMP_H

#include <stdio.h>
typedef struct dumpHandle {
	void (*dump2json)(FILE *fp, void *ptr);
	void (*listdump2json)(FILE *fp, size_t len, void *ptr);
} dumpHandle;

enum dumpType {
	DUMP2JSON,
};

void dump(dumpHandle *dh, FILE *fp, void *ptr, size_t size, enum dumpType);

#endif
